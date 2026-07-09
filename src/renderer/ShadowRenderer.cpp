#include "renderer/ShadowRenderer.h"

#include <algorithm>
#include <vector>

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "core/AppConfig.h"
#include "core/BBox3.h"
#include "core/Color.h"
#include "core/ViewFrustum.h"
#include "renderer/CascadeLightBasis.h"
#include "renderer/MeshRenderer.h"
#include "renderer/OmniLight.h"
#include "renderer/Renderable.h"
#include "renderer/ShadowPassInfos.h"

namespace renderer {

namespace {
constexpr int kShadowPassInfosSlot    = 6;
constexpr int kShadowPassInfosFloat4s = sizeof(ShadowPassInfos) / 16;  // 80/16 = 5

// Broad-phase-only Z half-extent for the CSM caster probe. The probe matrix
// is discarded after CullAndCollect (never used for rendering), so there is
// no depth-precision cost to being generous — it only needs to exceed the
// largest plausible distance, along the light direction, between a caster
// and the receiver slice it shadows.
constexpr float kCSMProbeZHalfExtent = 5000.f;

// Safety margin added around the tight caster/receiver Z union so geometry
// sitting exactly on the computed near/far plane isn't clipped by
// floating-point rounding.
constexpr float kCSMZFitEpsilon = 0.05f;
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

  // Break any feedback loops: slots 9–12 hold cascade/spot shadow maps, 13
  // holds cube shadow maps.  If any of these textures were read last frame they
  // must be unbound before we write into them via their FBOs.
  for (int s = 9; s <= 13; ++s) video_->UnbindSampler(s);

  shadow_pass_infos_cb_->Bind();

  // Back-face culling keeps front faces for single-sided meshes; shadow acne is
  // mitigated by the per-light shadow_bias rather than reverse culling.
  video_->SetFaceCulling(abstract::CullFace::kBack);
  video_->SetIndexType(abstract::IndexType::kUInt32);

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

  // Compute the per-cascade light-space basis (view matrix, XY bounds,
  // receiver Z range) and split depths. GlobalLight has no visibility into
  // scene casters, so it does not fit the final near/far — that happens
  // below, per cascade, from the actual casters found in the scene.
  std::array<CascadeLightBasis, kCSMCascadeCount> basis;
  light.ComputeCascadeBasis(camera, csm_infos_, basis.data());

  for (int i = 0; i < kCSMCascadeCount; ++i) {
    const CascadeLightBasis& b = basis[i];

    // Probe frustum: same XY bounds as the final cascade, but a deliberately
    // generous Z range. OrthoOffCenterRH's left/right/bottom/top clip planes
    // don't depend on z_near/z_far, so this finds every caster overlapping
    // the receiver's XY footprint regardless of its depth along the light
    // ray — i.e. an "infinite parallelepiped" fit extruded from the cascade.
    const core::Mat4f probe_ortho = core::Mat4f::OrthoOffCenterRH(
        b.min_x, b.max_x, b.min_y, b.max_y,
        -kCSMProbeZHalfExtent, kCSMProbeZHalfExtent);
    const core::ViewFrustum probe_frustum(probe_ortho * b.light_view);

    std::vector<Renderable*> casters;
    no_cull->CullAndCollect(probe_frustum, casters);
    octree->CullAndCollect(probe_frustum, casters);

    // Tight Z fit: union the receiver frustum's own Z range with every
    // candidate caster's light-space AABB. Only actual shadow casters count —
    // no_cull also carries non-caster renderables (e.g. GlobalLight itself,
    // always_visible with BBox3::kInfinite) that must not poison the fit.
    float min_z = b.receiver_min_z;
    float max_z = b.receiver_max_z;
    for (const Renderable* r : casters) {
      if (!r->IsShadowCaster()) continue;
      const core::BBox3 ls_bbox = r->GetWorldBBox() * b.light_view;
      min_z = std::min(min_z, ls_bbox.GetMin().z);
      max_z = std::max(max_z, ls_bbox.GetMax().z);
    }

    // May legitimately go non-positive: the light "eye" used to build
    // light_view is just an arbitrary reference frame now (not a hard near
    // origin), so a caster behind it in view space (max_z > 0) is expected
    // and fine — OrthoOffCenterRH only requires z_near != z_far.
    const float ls_near = -max_z - kCSMZFitEpsilon;
    const float ls_far  = -min_z + kCSMZFitEpsilon;

    const core::Mat4f ortho = core::Mat4f::OrthoOffCenterRH(
        b.min_x, b.max_x, b.min_y, b.max_y, ls_near, ls_far);
    const core::Mat4f vp = ortho * b.light_view;
    csm_infos_.cascade_vp[i] = vp;
    cascade_maps_[i]->SetLightVP(vp);

    ShadowPassInfos spi;
    spi.light_vp = vp;
    shadow_pass_infos_cb_->Fill(&spi);

    // `casters` was gathered against a superset (generous-Z) frustum sharing
    // the exact same XY planes as `ortho`, and ls_near/ls_far were derived
    // directly from these casters' own light-space Z extents (plus epsilon)
    // — every caster here is guaranteed to lie inside the final frustum too,
    // so re-running CullAndCollect against the tight frustum would return
    // the identical set and is skipped.
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
      spi.light_vp         = face_vp;
      spi.light_pos_range  = {pos.x, pos.y, pos.z, ol.GetRadius()};
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
      MeshRenderer::Instance().RenderDepthCube();

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
