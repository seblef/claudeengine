#include "renderer/LightRenderer.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/CullFace.h"
#include "abstract/Face.h"
#include "abstract/StencilOp.h"
#include "core/Vertex3D.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GlobalLight.h"
#include "renderer/LightInfos.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"
#include "renderer/Renderer.h"

namespace renderer {

namespace {

constexpr int kLightInfosSlot    = 4;
constexpr int kLightInfosFloat4s = sizeof(LightInfos) / 16;  // 80 / 16 = 5

// ---- Volume mesh builders ---------------------------------------------------

// Fullscreen quad in NDC: two triangles covering [-1,1]² at z=0.
// The vertex shader is expected to pass these clip-space positions through
// (or compute screen UVs from them) for GlobalLight.
std::unique_ptr<GeometryData> BuildQuad(abstract::VideoDevice* video) {
  // Vertex layout: position only — other fields zeroed.
  const core::Vertex3D verts[4] = {
    {{-1.f, -1.f, 0.f}, {}, {}, {}, {}},
    {{ 1.f, -1.f, 0.f}, {}, {}, {}, {}},
    {{ 1.f,  1.f, 0.f}, {}, {}, {}, {}},
    {{-1.f,  1.f, 0.f}, {}, {}, {}, {}},
  };
  const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
  return std::make_unique<GeometryData>(video, 4, verts, 2, idx);
}

// UV sphere: stacks latitude divisions, rings longitude divisions, radius=1.
// Stacks × rings generates (stacks+1)×(rings+1) vertices and
// stacks×rings×2 triangles.
std::unique_ptr<GeometryData> BuildSphere(abstract::VideoDevice* video,
                                           int stacks, int rings) {
  const int num_verts = (stacks + 1) * (rings + 1);
  const int num_tris  = stacks * rings * 2;

  std::vector<core::Vertex3D> verts;
  verts.reserve(num_verts);
  for (int s = 0; s <= stacks; ++s) {
    const float phi = static_cast<float>(M_PI) * static_cast<float>(s) /
                      static_cast<float>(stacks);
    const float sin_phi = std::sin(phi);
    const float cos_phi = std::cos(phi);
    for (int r = 0; r <= rings; ++r) {
      const float theta = 2.f * static_cast<float>(M_PI) *
                          static_cast<float>(r) / static_cast<float>(rings);
      const float x = sin_phi * std::cos(theta);
      const float y = cos_phi;
      const float z = sin_phi * std::sin(theta);
      verts.push_back({{x, y, z}, {}, {}, {}, {}});
    }
  }

  std::vector<uint16_t> idx;
  idx.reserve(num_tris * 3);
  for (int s = 0; s < stacks; ++s) {
    for (int r = 0; r < rings; ++r) {
      const uint16_t a = static_cast<uint16_t>(s * (rings + 1) + r);
      const uint16_t b = static_cast<uint16_t>(a + 1);
      const uint16_t c = static_cast<uint16_t>(a + rings + 1);
      const uint16_t d = static_cast<uint16_t>(c + 1);
      idx.insert(idx.end(), {a, c, b, b, c, d});
    }
  }

  return std::make_unique<GeometryData>(
      video, num_verts, verts.data(), num_tris, idx.data());
}

// Unit cone: apex at (0,0,0), base circle at z=1 with radius=1.
// N segments on the base circle; produces 2*N triangles (side + base cap).
std::unique_ptr<GeometryData> BuildCone(abstract::VideoDevice* video, int n) {
  // Vertices: apex, N base-circle points, base center.
  const int num_verts = n + 2;
  const int num_tris  = n * 2;

  std::vector<core::Vertex3D> verts;
  verts.reserve(num_verts);

  // Vertex 0: apex.
  verts.push_back({{0.f, 0.f, 0.f}, {}, {}, {}, {}});
  // Vertices 1..N: base circle.
  for (int i = 0; i < n; ++i) {
    const float a = 2.f * static_cast<float>(M_PI) * static_cast<float>(i) /
                    static_cast<float>(n);
    verts.push_back({{std::cos(a), std::sin(a), 1.f}, {}, {}, {}, {}});
  }
  // Vertex N+1: base center.
  verts.push_back({{0.f, 0.f, 1.f}, {}, {}, {}, {}});

  std::vector<uint16_t> idx;
  idx.reserve(num_tris * 3);
  for (int i = 0; i < n; ++i) {
    const uint16_t cur  = static_cast<uint16_t>(1 + i);
    const uint16_t next = static_cast<uint16_t>(1 + (i + 1) % n);
    // Side triangle (apex → edge).
    idx.insert(idx.end(), {0, cur, next});
    // Base triangle (center → edge, wound inward).
    idx.insert(idx.end(), {static_cast<uint16_t>(n + 1), next, cur});
  }

  return std::make_unique<GeometryData>(
      video, num_verts, verts.data(), num_tris, idx.data());
}

// Unit pyramid: apex at (0,0,0), rectangular base with half-extents ±0.5 at z=1.
// Produces 6 triangles (4 side + 2 base).
std::unique_ptr<GeometryData> BuildPyramid(abstract::VideoDevice* video) {
  // Base corners (CCW when viewed from -Z).
  const core::Vertex3D verts[5] = {
    {{ 0.f,  0.f, 0.f}, {}, {}, {}, {}},  // 0: apex
    {{-0.5f, -0.5f, 1.f}, {}, {}, {}, {}},  // 1: base BL
    {{ 0.5f, -0.5f, 1.f}, {}, {}, {}, {}},  // 2: base BR
    {{ 0.5f,  0.5f, 1.f}, {}, {}, {}, {}},  // 3: base TR
    {{-0.5f,  0.5f, 1.f}, {}, {}, {}, {}},  // 4: base TL
  };
  // Side faces (apex + two adjacent base corners).
  // Base face (2 triangles, CW so normal points toward −z).
  const uint16_t idx[18] = {
    0, 1, 2,  // side bottom
    0, 2, 3,  // side right
    0, 3, 4,  // side top
    0, 4, 1,  // side left
    1, 3, 2,  // base tri 0
    1, 4, 3,  // base tri 1
  };
  return std::make_unique<GeometryData>(video, 5, verts, 6, idx);
}

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

  switch (light.GetType()) {
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
}

}  // namespace

// ---- LightRenderer ----------------------------------------------------------

LightRenderer::LightRenderer(abstract::VideoDevice* video)
    : video_(video),
      vs_(video->CreateShader("lighting/light_pass_vs")),
      global_ps_(video->CreateShader("lighting/global_light_ps")),
      omni_ps_(video->CreateShader("lighting/omni_light_ps")),
      circle_spot_ps_(video->CreateShader("lighting/circle_spot_ps")),
      rect_spot_ps_(video->CreateShader("lighting/rect_spot_ps")),
      light_infos_cb_(video->CreateConstantBuffer(
          kLightInfosFloat4s, kLightInfosSlot, abstract::BufferUsage::kDynamic)),
      quad_(BuildQuad(video)),
      sphere_(BuildSphere(video, /*stacks=*/32, /*rings=*/64)),
      cone_(BuildCone(video, /*n=*/32)),
      pyramid_(BuildPyramid(video)) {}

LightRenderer::~LightRenderer() {
  vs_->Release();
  global_ps_->Release();
  omni_ps_->Release();
  circle_spot_ps_->Release();
  rect_spot_ps_->Release();
}

void LightRenderer::AddLight(Light* light) {
  instances_.push_back(light);
}

void LightRenderer::Render() {
  // Sort by type so GlobalLights come first and shader switches are minimised.
  std::stable_sort(instances_.begin(), instances_.end(),
                   [](const Light* a, const Light* b) {
                     return static_cast<int>(a->GetType()) <
                            static_cast<int>(b->GetType());
                   });

  light_infos_cb_->Bind();

  RenderGlobalLights();
  RenderLocalLights();
}

void LightRenderer::RenderGlobalLights() {
  video_->SetDepthTestEnabled(false);
  video_->SetStencilTestEnabled(false);
  video_->SetFaceCulling(abstract::CullFace::kNone);

  quad_->Set();

  const abstract::Shader* current_ps = nullptr;

  for (const Light* light : instances_) {
    if (light->GetType() != LightType::kGlobal) break;

    if (current_ps != global_ps_) {
      global_ps_->Activate();
      current_ps = global_ps_;
    }

    LightInfos infos;
    FillInfos(*light, &infos);
    light_infos_cb_->Fill(&infos);
    Renderer::Instance().SetRenderableInfos(light->GetVolumeMatrix());
    video_->RenderIndexed(quad_->GetNumIndices());
  }
}

void LightRenderer::RenderLocalLights() {
  for (const Light* light : instances_) {
    if (light->GetType() == LightType::kGlobal) continue;

    const GeometryData* geo = nullptr;
    abstract::Shader*    ps = nullptr;

    switch (light->GetType()) {
      case LightType::kOmni:
        geo = sphere_.get();
        ps  = omni_ps_;
        break;
      case LightType::kCircleSpot:
        geo = cone_.get();
        ps  = circle_spot_ps_;
        break;
      case LightType::kRectSpot:
        geo = pyramid_.get();
        ps  = rect_spot_ps_;
        break;
      default:
        continue;
    }

    LightInfos infos;
    FillInfos(*light, &infos);
    light_infos_cb_->Fill(&infos);
    Renderer::Instance().SetRenderableInfos(light->GetVolumeMatrix());

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
    geo->Set();
    video_->RenderIndexed(geo->GetNumIndices());

    // Sub-pass B: shade only stencil-marked pixels.
    ps->Activate();
    video_->SetColorWriteEnabled(true);
    video_->SetFaceCulling(abstract::CullFace::kBack);
    video_->SetDepthTestEnabled(false);
    video_->SetStencilFunc(abstract::CompareFunc::kNotEqual, 0, 0xFF);
    geo->Set();
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
