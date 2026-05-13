#include "renderer/ShadowRenderer.h"

#include <vector>

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "core/Color.h"
#include "core/ViewFrustum.h"
#include "renderer/MeshRenderer.h"
#include "renderer/Renderable.h"
#include "renderer/ShadowPassInfos.h"

namespace renderer {

namespace {
constexpr int kShadowPassInfosSlot    = 6;
constexpr int kShadowPassInfosFloat4s = sizeof(ShadowPassInfos) / 16;  // 64/16 = 4
}  // namespace

ShadowRenderer::ShadowRenderer(abstract::VideoDevice* video)
    : video_(video) {
  shadow_pass_infos_cb_ = video_->CreateConstantBuffer(
      kShadowPassInfosFloat4s, kShadowPassInfosSlot,
      abstract::BufferUsage::kDynamic);
}

ShadowRenderer::~ShadowRenderer() = default;

void ShadowRenderer::RenderShadowMaps(const std::vector<Light*>& lights,
                                      const IVisibilitySystem* no_cull,
                                      const IVisibilitySystem* octree) {
  shadow_pass_infos_cb_->Bind();

  // Front-face culling reduces shadow acne without a bias.
  video_->SetFaceCulling(abstract::CullFace::kFront);

  for (Light* light : lights) {
    // Lights that return nullopt do not support 2D shadow maps (GlobalLight
    // uses CSM; OmniLight uses a cube map — both deferred to later issues).
    const auto vp_opt = light->ComputeShadowVP();
    if (!vp_opt.has_value()) continue;
    const core::Mat4f& light_vp = *vp_opt;

    // Allocate a shadow map for this light if needed.
    auto it = shadow_maps_.find(light);
    if (it == shadow_maps_.end()) {
      auto result = shadow_maps_.emplace(
          light, std::make_unique<ShadowMap>(video_, kDefaultShadowResolution));
      it = result.first;
    }
    ShadowMap* smap = it->second.get();
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

const ShadowMap* ShadowRenderer::GetShadowMap(const Light* light) const {
  auto it = shadow_maps_.find(light);
  return (it != shadow_maps_.end()) ? it->second.get() : nullptr;
}

void ShadowRenderer::ClearShadowMaps() {
  shadow_maps_.clear();
}

}  // namespace renderer
