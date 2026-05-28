#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "abstract/ConstantBuffer.h"
#include "abstract/IndexBuffer.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Singleton.h"
#include "core/Vec3f.h"

namespace environment {

// Singleton renderer for a dynamic water surface at a configurable world-space height.
//
// Wave animation is driven by the WindInfos constant buffer (slot 7), which must
// already be bound when Render() is called.  The fragment shader performs Fresnel
// blending between a configurable deep-water colour and a sky zenith colour, and
// adds a Blinn-Phong specular highlight from the current sun direction.
//
// Outputs to the G-buffer (geometry pass), so water participates in the deferred
// lighting pass. Water does NOT render in the shadow depth pass — it receives
// shadows but does not cast them.
//
// Constant buffer slot 9 (WaterInfos): water_color, water_level,
//   sky_zenith_color, sun_direction.
//
// Usage:
//   new WaterRenderer();
//   WaterRenderer::Instance().Build(video, water_level);
//   // each frame (inside the geometry pass):
//   WaterRenderer::Instance().SetSkyZenithColor(r, g, b);
//   WaterRenderer::Instance().SetSunDirection(dir);
//   WaterRenderer::Instance().Render(camera);
//   WaterRenderer::Instance().Reset();  // release GPU resources
//   WaterRenderer::Shutdown();          // delete instance
class WaterRenderer : public core::Singleton<WaterRenderer> {
 public:
  WaterRenderer() = default;
  ~WaterRenderer() = default;

  WaterRenderer(const WaterRenderer&)            = delete;
  WaterRenderer& operator=(const WaterRenderer&) = delete;
  WaterRenderer(WaterRenderer&&)                 = delete;
  WaterRenderer& operator=(WaterRenderer&&)      = delete;

  // Creates a flat grid_size × grid_size mesh, loads water shaders, and
  // allocates the WaterInfos constant buffer at slot 9.
  // water_level: world-space Y of the undisplaced water surface.
  // grid_size:   number of quads per side (default 128).
  // video must outlive this renderer.
  void Build(abstract::VideoDevice* video, float water_level, int grid_size = 128);

  // Uploads the WaterInfos CB and draws the water grid into the G-buffer.
  // The WindInfos CB (slot 7) and SceneInfos CB (slot 2) must be bound by the
  // caller (Renderer::Update does this before the geometry pass).
  void Render(const core::Camera& camera);

  // Updates the world-space Y of the undisplaced surface (hot-path; no rebuild).
  void SetWaterLevel(float y) { water_level_ = y; }

  // Updates the sky zenith colour used for the Fresnel highlight.
  // Should be called once per frame from the renderer with the current sky colour.
  void SetSkyZenithColor(float r, float g, float b) {
    sky_zenith_r_ = r;
    sky_zenith_g_ = g;
    sky_zenith_b_ = b;
  }

  // Updates the world-space sun direction for Blinn-Phong specular.
  // Should be called once per frame from the renderer.
  void SetSunDirection(const core::Vec3f& dir) { sun_direction_ = dir; }

  // Releases all GPU resources. Called before Shutdown().
  void Reset();

  [[nodiscard]] bool IsReady() const { return shader_ != nullptr; }

 private:
  void BuildMesh(int grid_size);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                    video_   = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                         shader_  = nullptr;
  std::unique_ptr<abstract::ConstantBuffer> water_cb_;
  std::unique_ptr<abstract::VertexBuffer>   grid_vb_;
  std::unique_ptr<abstract::IndexBuffer>    grid_ib_;
  int                                       num_indices_ = 0;

  float         water_level_   = 0.f;
  float         sky_zenith_r_  = 0.40f;
  float         sky_zenith_g_  = 0.65f;
  float         sky_zenith_b_  = 0.90f;
  // cppcheck-suppress unusedStructMember
  core::Vec3f   sun_direction_ = {0.f, 1.f, 0.f};
};

}  // namespace environment
