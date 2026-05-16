#include "renderer/LightRenderer.h"

#include <algorithm>

#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/CullFace.h"
#include "abstract/Face.h"
#include "abstract/StencilOp.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GeometryUtils.h"
#include "renderer/GlobalLight.h"
#include "renderer/LightInfos.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"
#include "renderer/Renderer.h"
#include "renderer/CSMInfos.h"
#include "renderer/ShadowCubeMap.h"
#include "renderer/ShadowMap.h"
#include "renderer/ShadowRenderer.h"

namespace renderer {

namespace {

constexpr int kLightInfosSlot    = 4;
constexpr int kLightInfosFloat4s = sizeof(LightInfos) / 16;  // 160 / 16 = 10

// ---- LightInfos filling helpers ---------------------------------------------

void FillCommonInfos(const Light& light, LightInfos* infos) {
  const core::Color& c = light.GetColor();
  infos->cr        = c.r;
  infos->cg        = c.g;
  infos->cb        = c.b;
  infos->intensity = light.GetIntensity();
  infos->type      = static_cast<float>(static_cast<int>(light.GetType()));

  const core::Mat4f& wm = light.GetWorldMatrix();
  infos->px = wm(0, 3);
  infos->py = wm(1, 3);
  infos->pz = wm(2, 3);
}

void FillInfos(const Light& light, LightInfos* infos) {
  *infos = {};
  FillCommonInfos(light, infos);

  const LightType ltype = light.GetType();
  switch (ltype) {
    case LightType::kGlobal: {
      const auto& gl  = static_cast<const GlobalLight&>(light);
      const auto& dir = gl.GetDirection();
      const auto& amb = gl.GetAmbientColor();
      infos->dx = dir.x;  infos->dy = dir.y;  infos->dz = dir.z;
      infos->ar = amb.x;  infos->ag = amb.y;  infos->ab = amb.z;
      break;
    }
    case LightType::kOmni: {
      const auto& ol = static_cast<const OmniLight&>(light);
      infos->range = ol.GetRadius();
      break;
    }
    case LightType::kCircleSpot: {
      const auto& sl  = static_cast<const CircleSpotLight&>(light);
      const auto& dir = sl.GetDirection();
      infos->dx          = dir.x;  infos->dy = dir.y;  infos->dz = dir.z;
      infos->range       = sl.GetRange();
      infos->inner_angle = sl.GetInnerAngle();
      infos->outer_angle = sl.GetOuterAngle();
      break;
    }
    case LightType::kRectSpot: {
      const auto& rl  = static_cast<const RectangleSpotLight&>(light);
      const auto& dir = rl.GetDirection();
      infos->dx      = dir.x;  infos->dy = dir.y;  infos->dz = dir.z;
      infos->range   = rl.GetRange();
      infos->h_angle = rl.GetHAngle();
      infos->v_angle = rl.GetVAngle();
      break;
    }
  }

  // Shadow fields: cast_shadow drives the shader's shadow sample; light_vp is
  // only used when cast_shadow == 1.0.
  if (ltype == LightType::kOmni) {
    const ShadowCubeMap* csmap = ShadowRenderer::Instance().GetShadowCubeMap(&light);
    infos->cast_shadow = csmap ? 1.0f : 0.0f;
  } else if(ltype == LightType::kCircleSpot || ltype == LightType::kRectSpot) {
    const ShadowMap* smap = ShadowRenderer::Instance().GetShadowMap(&light);
    infos->cast_shadow = smap ? 1.0f : 0.0f;
    if (smap) infos->light_vp = smap->GetLightVP();
  }
  infos->shadow_bias = light.GetShadowBias();
}

}  // namespace

// ---- LightRenderer ----------------------------------------------------------

LightRenderer::LightRenderer(abstract::VideoDevice* video)
    : video_(video),
      global_shader_(video->CreateShader("lighting/global_light")),
      omni_shader_(video->CreateShader("lighting/omni_light")),
      circle_spot_shader_(video->CreateShader("lighting/circle_spot")),
      rect_spot_shader_(video->CreateShader("lighting/rect_spot")),
      light_infos_cb_(video->CreateConstantBuffer(
          kLightInfosFloat4s, kLightInfosSlot, abstract::BufferUsage::kDynamic)),
      quad_(CreateQuad(video)),
      sphere_(CreateSphere(video, /*stacks=*/32, /*rings=*/64)),
      cone_(CreateCone(video, /*n=*/32)),
      pyramid_(CreatePyramid(video)) {}

LightRenderer::~LightRenderer() {
  global_shader_->Release();
  omni_shader_->Release();
  circle_spot_shader_->Release();
  rect_spot_shader_->Release();
}

void LightRenderer::AddLight(Light* light) {
  instances_.push_back(light);
}

void LightRenderer::Render(bool disable_shadows) {
  // Sort by type so GlobalLights come first and shader switches are minimised.
  std::stable_sort(instances_.begin(), instances_.end(),
                   [](const Light* a, const Light* b) {
                     return static_cast<int>(a->GetType()) <
                            static_cast<int>(b->GetType());
                   });

  light_infos_cb_->Bind();

  RenderGlobalLights(disable_shadows);
  RenderLocalLights(disable_shadows);
}

void LightRenderer::RenderGlobalLights(bool disable_shadows) {
  video_->SetDepthTestEnabled(false);
  video_->SetStencilTestEnabled(false);
  video_->SetFaceCulling(abstract::CullFace::kNone);

  // Bind cascade shadow maps (samplers 9–12); null cascades use sampler 0 (white).
  for (int i = 0; i < kCSMCascadeCount; ++i) {
    const ShadowMap* cm = ShadowRenderer::Instance().GetCascadeMap(i);
    if (cm) cm->GetDepthRT()->BindAsSampler(9 + i);
  }

  // Shader and geometry are the same for all GlobalLights — activate once.
  global_shader_->Activate();
  quad_->Set();

  for (const Light* light : instances_) {
    if (light->GetType() != LightType::kGlobal) continue;

    LightInfos infos;
    FillInfos(*light, &infos);
    // CSM availability drives cast_shadow for GlobalLight, not the 2D pool.
    infos.cast_shadow =
        (!disable_shadows && ShadowRenderer::Instance().HasCSM()) ? 1.0f : 0.0f;
    light_infos_cb_->Fill(&infos);
    Renderer::Instance().SetRenderableInfos(light->GetVolumeMatrix());
    video_->RenderIndexed(quad_->GetNumIndices());
  }
}

void LightRenderer::RenderLocalLights(bool disable_shadows) {
  // Instances are sorted by type, so shader and geometry only change at type
  // boundaries.  Track the current bindings to avoid redundant state switches.
  const abstract::Shader* current_shader = nullptr;
  const GeometryData*     current_geo    = nullptr;

  for (const Light* light : instances_) {
    if (light->GetType() == LightType::kGlobal) continue;

    abstract::Shader*   shader = nullptr;
    const GeometryData* geo    = nullptr;

    switch (light->GetType()) {
      case LightType::kOmni:
        shader = omni_shader_;
        geo    = sphere_.get();
        break;
      case LightType::kCircleSpot:
        shader = circle_spot_shader_;
        geo    = cone_.get();
        break;
      case LightType::kRectSpot:
        shader = rect_spot_shader_;
        geo    = pyramid_.get();
        break;
      default:
        continue;
    }

    // Fill per-light constant buffer before both sub-passes.
    LightInfos infos;
    FillInfos(*light, &infos);
    if (disable_shadows) infos.cast_shadow = 0.0f;
    light_infos_cb_->Fill(&infos);
    Renderer::Instance().SetRenderableInfos(light->GetVolumeMatrix());

    // Switch shader and geometry only when the light type changes.
    if (shader != current_shader) {
      shader->Activate();
      current_shader = shader;
    }
    if (geo != current_geo) {
      geo->Set();
      current_geo = geo;
    }

    // Bind shadow depth RT at sampler 9 for spot lights that cast shadows.
    if (light->GetType() == LightType::kCircleSpot ||
        light->GetType() == LightType::kRectSpot) {
      const ShadowMap* smap = ShadowRenderer::Instance().GetShadowMap(light);
      if (smap) smap->GetDepthRT()->BindAsSampler(9);
    }

    // Bind cube shadow map at sampler 13 for omni lights that cast shadows.
    if (light->GetType() == LightType::kOmni) {
      const ShadowCubeMap* scm = ShadowRenderer::Instance().GetShadowCubeMap(light);
      if (scm) scm->GetCubeRT()->BindAsSampler(13);
    }

    // Sub-pass A: mark pixels inside the light volume in the stencil buffer.
    video_->ClearStencil(0);
    video_->SetColorWriteEnabled(false);
    video_->SetDepthTestEnabled(true);
    video_->SetFaceCulling(abstract::CullFace::kNone);
    video_->SetStencilTestEnabled(true);
    video_->SetStencilFunc(abstract::CompareFunc::kAlways, 0, 0xFF);
    video_->SetStencilOp(abstract::Face::kFront,
                         abstract::StencilOp::kKeep,
                         abstract::StencilOp::kIncrWrap,
                         abstract::StencilOp::kKeep);
    video_->SetStencilOp(abstract::Face::kBack,
                         abstract::StencilOp::kKeep,
                         abstract::StencilOp::kDecrWrap,
                         abstract::StencilOp::kKeep);
    video_->RenderIndexed(geo->GetNumIndices());

    // Sub-pass B: shade only stencil-marked pixels (geometry already bound).
    // kFront culling (not kBack) is intentional: the sphere/cone volumes have
    // their outer faces as GL_BACK (sphere: far hemisphere, cone: side surface).
    // When the camera is inside the light volume all sphere faces appear as
    // GL_BACK (outward winding reversed), so kBack would cull everything and
    // produce no fragments.  kFront correctly renders the outer faces in both
    // camera-outside and camera-inside cases.
    video_->SetColorWriteEnabled(true);
    video_->SetFaceCulling(abstract::CullFace::kBack);
    video_->SetDepthTestEnabled(false);
    video_->SetStencilFunc(abstract::CompareFunc::kNotEqual, 0, 0xFF);
    video_->RenderIndexed(geo->GetNumIndices());

    video_->SetStencilTestEnabled(false);
  }

  // Restore default state.
  video_->SetFaceCulling(abstract::CullFace::kBack);
  video_->SetColorWriteEnabled(true);
  video_->SetDepthTestEnabled(true);
}

void LightRenderer::EndRender() {
  instances_.clear();
}

}  // namespace renderer
