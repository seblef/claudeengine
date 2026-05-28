#include "environment/SkyRenderer.h"

#include <cmath>
#include <cstdint>

#include "abstract/BufferUsage.h"
#include "abstract/CompareFunc.h"
#include "abstract/IndexType.h"
#include "core/Vec3f.h"
#include "core/Vertex3D.h"
#include "core/VertexType.h"
#include "environment/SkyInfos.h"

namespace environment {

namespace {
constexpr int   kSkyInfosSlot    = 8;
constexpr int   kSkyInfosFloat4s = sizeof(SkyInfos) / 16;  // 32 / 16 = 2
constexpr float kPi              = 3.14159265f;
constexpr float kSunriseHour     = 6.f;
constexpr float kSunsetHour      = 18.f;
// Maximum solar elevation at latitude 45° N on equinox.
constexpr float kMaxElevationRad = 45.f * (kPi / 180.f);
}  // namespace

void SkyRenderer::Build(abstract::VideoDevice* video) {
  video_  = video;
  shader_ = video_->CreateShader("sky/sky");

  sky_cb_ = video_->CreateConstantBuffer(
      kSkyInfosFloat4s, kSkyInfosSlot, abstract::BufferUsage::kDynamic);

  // Fullscreen quad covering NDC [-1, 1]² at z=0 (VS moves it to far plane).
  const core::Vertex3D verts[4] = {
    {{-1.f, -1.f, 0.f}, {}, {}, {}, {}},
    {{ 1.f, -1.f, 0.f}, {}, {}, {}, {}},
    {{ 1.f,  1.f, 0.f}, {}, {}, {}, {}},
    {{-1.f,  1.f, 0.f}, {}, {}, {}, {}},
  };
  const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};

  quad_vb_ = video_->CreateVertexBuffer(
      core::VertexType::k3D, 4,
      abstract::BufferUsage::kImmutable, verts);
  quad_ib_ = video_->CreateIndexBuffer(
      abstract::IndexType::kUInt16, 6,
      abstract::BufferUsage::kImmutable, idx);
}

void SkyRenderer::Render([[maybe_unused]] const core::Camera& camera,
                          float world_time) {
  if (!shader_) return;

  SkyInfos si;
  si.sun_direction = ComputeSunDirection(world_time);
  si.time_of_day   = world_time;
  si.turbidity     = turbidity_;

  sky_cb_->Bind();
  sky_cb_->Fill(&si);

  video_->SetDepthWriteEnabled(false);
  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);

  shader_->Activate();
  quad_vb_->Bind();
  quad_ib_->Bind();
  video_->RenderIndexed(6);

  video_->SetDepthFunc(abstract::CompareFunc::kLess);
}

void SkyRenderer::Reset() {
  if (shader_) {
    shader_->Release();
    shader_ = nullptr;
  }
  sky_cb_.reset();
  quad_vb_.reset();
  quad_ib_.reset();
  video_ = nullptr;
}

core::Vec3f SkyRenderer::ComputeSunDirection(float time_of_day) {
  // Matches WorldTime::GetSunDirection(): sun arcs from east at 06:00,
  // peaks at latitude 45° N elevation at 12:00, sets in the west at 18:00.
  const float azimuth   = ((time_of_day - kSunriseHour) / 24.f) * 2.f * kPi;
  const float elevation = std::sin(((time_of_day - kSunriseHour) / 12.f) * kPi)
                          * kMaxElevationRad;
  const float cos_el = std::cos(elevation);
  const float x = -std::cos(azimuth) * cos_el;
  const float y =  std::sin(elevation);
  const float z = -std::sin(azimuth) * cos_el;
  return core::Vec3f(x, y, z).Normalized();
}

}  // namespace environment
