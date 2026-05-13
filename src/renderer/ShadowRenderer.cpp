#include "renderer/ShadowRenderer.h"

#include <cmath>
#include <vector>

#include "abstract/BufferUsage.h"
#include "abstract/CullFace.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "core/ViewFrustum.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/RectangleSpotLight.h"
#include "renderer/Renderable.h"
#include "renderer/ShadowPassInfos.h"

namespace renderer {

namespace {
constexpr int kShadowPassInfosSlot    = 6;
constexpr int kShadowPassInfosFloat4s = sizeof(ShadowPassInfos) / 16;  // 64/16 = 4
constexpr float kShadowNear           = 0.1f;
}  // namespace

ShadowRenderer::ShadowRenderer(abstract::VideoDevice* video)
    : video_(video),
      depth_shader_(video->CreateShader("shadow_depth")) {
  shadow_pass_infos_cb_ = video_->CreateConstantBuffer(
      kShadowPassInfosFloat4s, kShadowPassInfosSlot,
      abstract::BufferUsage::kDynamic);
}

ShadowRenderer::~ShadowRenderer() {
  depth_shader_->Release();
}

core::Mat4f ShadowRenderer::ComputeLightVP(const Light* light) {
  const core::Mat4f& wm = light->GetWorldMatrix();
  const core::Vec3f  pos(wm(0, 3), wm(1, 3), wm(2, 3));

  float fov_y  = 0.f;
  float aspect = 1.f;
  float range  = 1.f;
  core::Vec3f dir;

  if (light->GetType() == LightType::kCircleSpot) {
    const auto* sl = static_cast<const CircleSpotLight*>(light);
    fov_y  = 2.f * sl->GetOuterAngle();
    range  = sl->GetRange();
    dir    = sl->GetDirection();
  } else {
    const auto* sl = static_cast<const RectangleSpotLight*>(light);
    const float h  = sl->GetHAngle();
    const float v  = sl->GetVAngle();
    fov_y  = 2.f * v;
    aspect = std::tan(h) / std::tan(v);
    range  = sl->GetRange();
    dir    = sl->GetDirection();
  }

  // Choose an up vector that is not collinear with dir.
  const core::Vec3f up =
      (std::abs(dir.y) > 0.9f) ? core::Vec3f(1.f, 0.f, 0.f)
                                : core::Vec3f(0.f, 1.f, 0.f);

  const core::Mat4f view = core::Mat4f::LookAtRH(pos, pos + dir, up);
  const core::Mat4f proj =
      core::Mat4f::PerspectiveRH(fov_y, aspect, kShadowNear, range);

  return view * proj;
}

void ShadowRenderer::RenderShadowMaps(const std::vector<Light*>& lights,
                                      const IVisibilitySystem* no_cull,
                                      const IVisibilitySystem* octree) {
  shadow_pass_infos_cb_->Bind();
  depth_shader_->Activate();

  // Front-face culling reduces shadow acne without a bias.
  video_->SetFaceCulling(abstract::CullFace::kFront);

  for (Light* light : lights) {
    // Skip light types not handled in this issue.
    if (light->GetType() == LightType::kGlobal ||
        light->GetType() == LightType::kOmni) {
      continue;
    }

    // Allocate a shadow map for this light if needed.
    auto it = shadow_maps_.find(light);
    if (it == shadow_maps_.end()) {
      auto result = shadow_maps_.emplace(
          light, std::make_unique<ShadowMap>(video_, kDefaultShadowResolution));
      it = result.first;
    }
    ShadowMap* smap = it->second.get();

    // Compute and store the light-space VP matrix.
    const core::Mat4f light_vp = ComputeLightVP(light);
    smap->SetLightVP(light_vp);

    // Upload to the shadow-pass CB.
    ShadowPassInfos spi;
    spi.light_vp = light_vp;
    shadow_pass_infos_cb_->Fill(&spi);

    // Collect shadow casters visible from this light.
    const core::ViewFrustum light_frustum(light_vp);
    std::vector<Renderable*> casters;
    no_cull->CullAndCollect(light_frustum, casters);
    octree->CullAndCollect(light_frustum, casters);

    // Render depth pass.
    const int res = smap->GetResolution();
    smap->GetFBO()->BindForWriting();
    video_->SetViewport(0, 0, res, res);
    video_->SetDepthTestEnabled(true);
    video_->SetDepthWriteEnabled(true);
    video_->ClearRenderTargets(core::Color::kBlack);

    for (Renderable* r : casters) {
      if (!r->IsShadowCaster()) continue;
      r->DrawDepth(video_);
    }

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
