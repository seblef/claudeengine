#include "renderer/ShadowRenderer.h"

#include <algorithm>
#include <vector>

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "core/AppConfig.h"
#include "core/Color.h"
#include "core/ViewFrustum.h"
#include "renderer/MeshRenderer.h"
#include "renderer/OmniLight.h"
#include "renderer/Renderable.h"
#include "renderer/ShadowPassInfos.h"

namespace renderer {

namespace {
constexpr int kShadowPassInfosSlot    = 6;
constexpr int kShadowPassInfosFloat4s = sizeof(ShadowPassInfos) / 16;  // 64/16 = 4
}  // namespace

ShadowRenderer::ShadowRenderer(abstract::VideoDevice* video)
    : video_(video),
      pool_(video, core::AppConfig::GetShadows()),
      csm_infos_{} {
  shadow_pass_infos_cb_ = video_->CreateConstantBuffer(
      kShadowPassInfosFloat4s, kShadowPassInfosSlot,
      abstract::BufferUsage::kDynamic);
}

ShadowRenderer::~ShadowRenderer() = default;

void ShadowRenderer::RenderShadowMaps(const std::vector<Light*>& lights,
                                      const IVisibilitySystem*   no_cull,
                                      const IVisibilitySystem*   octree,
                                      const core::Camera&        camera) {
  pool_.Assign(lights, camera);

  shadow_pass_infos_cb_->Bind();

  // Front-face culling reduces shadow acne without a bias.
  video_->SetFaceCulling(abstract::CullFace::kFront);

  // Render CSM cascades for the scene's GlobalLight (if any, and cast_shadow=true).
  has_csm_ = false;
  const auto global_it = std::find_if(lights.cbegin(), lights.cend(),
      [](const Light* l) {
        return l->GetType() == LightType::kGlobal && l->GetCastShadow();
      });
  if (global_it != lights.cend()) {
    has_csm_ = true;
    RenderCascades(static_cast<const GlobalLight&>(**global_it),
                   no_cull, octree, camera);
  }

  // Render cube shadow maps for omni lights from the pool.
  RenderCubeShadows(lights, no_cull, octree);

  // Render 2D shadow maps for spot lights from the pool.
  for (const Light* light : lights) {
    if (!light->GetCastShadow()) continue;

    // Lights not assigned a pool slot receive no shadow this frame.
    ShadowMap* smap = const_cast<ShadowMap*>(pool_.GetShadowMap(light));
    if (!smap) continue;

    // Lights that return nullopt do not support 2D shadow maps (GlobalLight
    // uses CSM; OmniLight uses a cube map — both handled separately).
    const auto vp_opt = light->ComputeShadowVP();
    if (!vp_opt.has_value()) continue;
    const core::Mat4f& light_vp = *vp_opt;

    smap->SetLightVP(light_vp);

    // Upload light-space VP to CB slot 6.
    ShadowPassInfos spi;
    spi.light_vp = light_vp;
    shadow_pass_infos_cb_->Fill(&spi);

    // Collect shadow casters visible from this light's frustum.
    const core::ViewFrustum light_frustum(light_vp);
    std::vector<Renderable*> casters;
    no_cull->CullAndCollect(light_frustum, casters);
    octree->CullAndCollect(light_frustum, casters);

    // Enqueue casters and render the depth pass.
    const int res = smap->GetResolution();
    smap->GetFBO()->BindForWriting();
    video_->SetViewport(0, 0, res, res);
    video_->SetDepthTestEnabled(true);
    video_->SetDepthWriteEnabled(true);
    video_->ClearRenderTargets(core::Color::kBlack);

    for (Renderable* r : casters) r->EnqueueDepth();
    MeshRenderer::Instance().RenderDepth();

    smap->GetFBO()->UnbindForWriting();
  }

  // Restore back-face culling and full-screen viewport.
  video_->SetFaceCulling(abstract::CullFace::kBack);
  video_->SetViewport(0, 0, video_->GetWidth(), video_->GetHeight());
}

void ShadowRenderer::RenderCascades(const GlobalLight&       light,
                                    const IVisibilitySystem* no_cull,
                                    const IVisibilitySystem* octree,
                                    const core::Camera&      camera) {
  const int res = light.GetShadowResolution();

  // Lazily allocate or reallocate cascade shadow maps when resolution changes.
  if (!cascade_maps_[0] || cascade_maps_[0]->GetResolution() != res) {
    for (int i = 0; i < kCSMCascadeCount; ++i)
      cascade_maps_[i] = std::make_unique<ShadowMap>(video_, res);
  }

  // Compute the 4 cascade VP matrices and split depths.
  light.ComputeCascadeMatrices(camera, csm_infos_);

  for (int i = 0; i < kCSMCascadeCount; ++i) {
    const core::Mat4f& vp = csm_infos_.cascade_vp[i];
    cascade_maps_[i]->SetLightVP(vp);

    ShadowPassInfos spi;
    spi.light_vp = vp;
    shadow_pass_infos_cb_->Fill(&spi);

    // Cull shadow casters visible from this cascade frustum.
    const core::ViewFrustum cascade_frustum(vp);
    std::vector<Renderable*> casters;
    no_cull->CullAndCollect(cascade_frustum, casters);
    octree->CullAndCollect(cascade_frustum, casters);

    cascade_maps_[i]->GetFBO()->BindForWriting();
    video_->SetViewport(0, 0, res, res);
    video_->SetDepthTestEnabled(true);
    video_->SetDepthWriteEnabled(true);
    video_->ClearRenderTargets(core::Color::kBlack);

    for (Renderable* r : casters) r->EnqueueDepth();
    MeshRenderer::Instance().RenderDepth();

    cascade_maps_[i]->GetFBO()->UnbindForWriting();
  }
}

void ShadowRenderer::RenderCubeShadows(const std::vector<Light*>& lights,
                                       const IVisibilitySystem*   no_cull,
                                       const IVisibilitySystem*   octree) {
  for (const Light* light : lights) {
    if (light->GetType() != LightType::kOmni) continue;
    if (!light->GetCastShadow()) continue;

    ShadowCubeMap* scm = const_cast<ShadowCubeMap*>(pool_.GetShadowCubeMap(light));
    if (!scm) continue;

    const auto& ol  = static_cast<const OmniLight&>(*light);
    const core::Mat4f& wm = ol.GetWorldMatrix();
    const core::Vec3f  pos(wm(0, 3), wm(1, 3), wm(2, 3));
    scm->ComputeFaceMatrices(pos, ol.GetRadius());

    for (int face = 0; face < 6; ++face) {
      const core::Mat4f& face_vp = scm->GetLightVP(face);

      ShadowPassInfos spi;
      spi.light_vp = face_vp;
      shadow_pass_infos_cb_->Fill(&spi);

      const core::ViewFrustum face_frustum(face_vp);
      std::vector<Renderable*> casters;
      no_cull->CullAndCollect(face_frustum, casters);
      octree->CullAndCollect(face_frustum, casters);

      const int size = scm->GetSize();
      scm->BindFaceForWriting(face);
      video_->SetViewport(0, 0, size, size);
      video_->SetDepthTestEnabled(true);
      video_->SetDepthWriteEnabled(true);
      video_->ClearRenderTargets(core::Color::kBlack);

      for (Renderable* r : casters) r->EnqueueDepth();
      MeshRenderer::Instance().RenderDepth();

      scm->UnbindForWriting();
    }
  }
}

const ShadowMap* ShadowRenderer::GetShadowMap(const Light* light) const {
  return pool_.GetShadowMap(light);
}

const ShadowCubeMap* ShadowRenderer::GetShadowCubeMap(const Light* light) const {
  return pool_.GetShadowCubeMap(light);
}

const ShadowMap* ShadowRenderer::GetCascadeMap(int index) const {
  if (!has_csm_ || index < 0 || index >= kCSMCascadeCount) return nullptr;
  return cascade_maps_[index].get();
}

void ShadowRenderer::ClearShadowMaps() {
  pool_.ClearAll();
}

}  // namespace renderer
