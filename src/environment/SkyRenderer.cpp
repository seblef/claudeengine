#include "environment/SkyRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

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
constexpr int   kSkyInfosFloat4s = sizeof(SkyInfos) / 16;
constexpr float kPi              = 3.14159265f;
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
  si.sun_direction    = ComputeSunDirection(world_time);
  si.time_of_day      = world_time;
  si.turbidity        = turbidity_;
  si.has_moon_texture = moon_tex_ ? 1.f : 0.f;

  sky_cb_->Bind();
  sky_cb_->Fill(&si);

  if (moon_tex_) moon_tex_->Bind(0);

  video_->SetDepthWriteEnabled(false);
  video_->SetDepthTestEnabled(true);
  video_->SetDepthFunc(abstract::CompareFunc::kLessEqual);
  video_->SetIndexType(abstract::IndexType::kUInt16);

  shader_->Activate();
  quad_vb_->Bind();
  quad_ib_->Bind();
  video_->RenderIndexed(6);

  video_->UnbindSampler(0);
  video_->SetDepthFunc(abstract::CompareFunc::kLess);
}

void SkyRenderer::SetMoonTexture(const std::string& path) {
  if (moon_tex_) {
    moon_tex_->Release();
    moon_tex_ = nullptr;
  }
  if (!path.empty() && video_) {
    moon_tex_ = video_->CreateTexture(path);
  }
}

void SkyRenderer::Reset() {
  if (moon_tex_) {
    moon_tex_->Release();
    moon_tex_ = nullptr;
  }
  if (shader_) {
    shader_->Release();
    shader_ = nullptr;
  }
  sky_cb_.reset();
  quad_vb_.reset();
  quad_ib_.reset();
  video_ = nullptr;
}

core::Vec3f SkyRenderer::ComputeSunDirection(float time_of_day) const {
  const float phi   = latitude_deg_    * (kPi / 180.f);  // latitude (rad)
  const float delta = declination_deg_ * (kPi / 180.f);  // declination (rad)

  // Hour angle: 0 at solar noon, negative in the morning.
  const float H = (time_of_day - 12.f) * (kPi / 12.f);

  // Solar elevation   sin(α) = sin(φ)·sin(δ) + cos(φ)·cos(δ)·cos(H)
  const float sin_elev = std::sin(phi) * std::sin(delta)
                       + std::cos(phi) * std::cos(delta) * std::cos(H);
  const float elevation = std::asin(std::clamp(sin_elev, -1.f, 1.f));
  const float cos_el    = std::cos(elevation);

  // Solar azimuth from north (clockwise viewed from above):
  //   cos(A) = (sin(δ) − sin(φ)·sin(α)) / (cos(φ)·cos(α))
  // Morning (H < 0): A < π (east side); afternoon (H > 0): A > π (west side).
  float azimuth = 0.f;
  if (cos_el > 1e-4f) {
    const float cos_az = std::clamp(
        (std::sin(delta) - std::sin(phi) * sin_elev)
            / (std::cos(phi) * cos_el),
        -1.f, 1.f);
    azimuth = (H <= 0.f) ? std::acos(cos_az) : 2.f * kPi - std::acos(cos_az);
  }

  // Engine coordinates (+X east, +Y up, +Z south):
  // A=0→north(−Z), A=π/2→east(+X), A=π→south(+Z), A=3π/2→west(−X).
  const float x =  std::sin(azimuth) * cos_el;
  const float y =  std::sin(elevation);
  const float z = -std::cos(azimuth) * cos_el;
  return core::Vec3f(x, y, z).Normalized();
}

}  // namespace environment
