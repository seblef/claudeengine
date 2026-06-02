#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "abstract/IndexBuffer.h"
#include "abstract/RawTexture.h"
#include "abstract/RenderTarget.h"
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
// blending, depth-based absorption, refraction from the scene colour snapshot,
// Cook-Torrance GGX specular from the sun direction, and physically-motivated foam
// (wave-height, surface steepness, and animated shoreline ring).
//
// Renders in the FORWARD pass (after deferred lighting + emissive) using
// SrcAlpha / OneMinusSrcAlpha blending.  The WaterInfos constant buffer at
// slot 9 is owned by Renderer and must be bound before Render() is called.
//
// Constant buffer slot 9 (WaterInfos): bound globally by Renderer.
// Sampler slot 0: procedural water normal map (RGBA8, tileable).
// Sampler slot 1: procedural foam texture    (RGBA8, tileable).
// Sampler slot 2: scene_color snapshot (RGBA16F).
// Sampler slot 3: depth snapshot (DEPTH24STENCIL8).
//
// Usage:
//   new WaterRenderer();
//   WaterRenderer::Instance().Build(video, water_level);
//   // each frame (after emissive pass, inside emissive FBO):
//   WaterRenderer::Instance().SetSkyZenithColor(r, g, b);
//   WaterRenderer::Instance().SetSunDirection(dir);
//   WaterRenderer::Instance().Render(camera, scene_color_rt, depth_rt);
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

  // Creates a flat water mesh and loads water shaders.
  // water_level:          world-space Y of the undisplaced water surface.
  // terrain_world_width:  world-space width of the terrain (X axis). Pass 0 for a
  //                       fixed-size mesh centred at the world origin.
  // terrain_world_height: world-space depth of the terrain (Z axis). Pass 0 for a
  //                       fixed-size mesh centred at the world origin.
  // When non-zero terrain dimensions are provided the mesh is centred at the
  // terrain's midpoint and extends kTerrainMarginFactor times beyond its edges.
  // grid_size: number of quads per side when terrain dimensions are not supplied.
  // video must outlive this renderer.
  void Build(abstract::VideoDevice* video, float water_level,
             float terrain_world_width = 0.f, float terrain_world_height = 0.f,
             int grid_size = 128);

  // Draws the water grid into the currently bound FBO (emissive FBO).
  // scene_color: HDR snapshot taken just before this call (sampler slot 2).
  // depth:       depth snapshot taken just before this call (sampler slot 3).
  // The WaterInfos CB (slot 9) and SceneInfos CB (slot 2) must already be bound.
  void Render(const core::Camera& camera,
              abstract::RenderTarget* scene_color,
              abstract::RenderTarget* depth);

  // Updates the world-space Y of the undisplaced surface (hot-path; no rebuild).
  void SetWaterLevel(float y) { water_level_ = y; }

  // Updates the sky zenith colour used for Fresnel reflection.
  void SetSkyZenithColor(float r, float g, float b) {
    sky_zenith_r_ = r;
    sky_zenith_g_ = g;
    sky_zenith_b_ = b;
  }

  // Updates the world-space sun direction for Blinn-Phong specular.
  void SetSunDirection(const core::Vec3f& dir) { sun_direction_ = dir; }

  // Returns all water parameters packed into WaterInfos for upload by Renderer.
  [[nodiscard]] float GetWaterLevel()   const { return water_level_; }
  [[nodiscard]] float GetSkyZenithR()   const { return sky_zenith_r_; }
  [[nodiscard]] float GetSkyZenithG()   const { return sky_zenith_g_; }
  [[nodiscard]] float GetSkyZenithB()   const { return sky_zenith_b_; }
  [[nodiscard]] const core::Vec3f& GetSunDirection() const { return sun_direction_; }

  // Releases all GPU resources. Called before Shutdown().
  void Reset();

  [[nodiscard]] bool IsReady() const { return shader_ != nullptr; }

 private:
  // Fraction by which the water plane extends beyond each terrain edge.
  // 1.2 means the water is 20 % wider/deeper than the terrain on every side.
  // cppcheck-suppress unusedStructMember
  static constexpr float kTerrainMarginFactor = 1.2f;

  void BuildMesh(int grid_size);
  void BuildNormalMap();
  void BuildFoamTexture();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                  video_   = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                       shader_  = nullptr;
  std::unique_ptr<abstract::VertexBuffer> grid_vb_;
  std::unique_ptr<abstract::IndexBuffer>  grid_ib_;
  std::unique_ptr<abstract::RawTexture>   normal_map_tex_;
  std::unique_ptr<abstract::RawTexture>   foam_tex_;
  int                                     num_indices_ = 0;

  // cppcheck-suppress unusedStructMember
  float         terrain_world_width_  = 0.f;
  // cppcheck-suppress unusedStructMember
  float         terrain_world_height_ = 0.f;
  float         water_level_          = 0.f;
  float         sky_zenith_r_  = 0.40f;
  float         sky_zenith_g_  = 0.65f;
  float         sky_zenith_b_  = 0.90f;
  // cppcheck-suppress unusedStructMember
  core::Vec3f   sun_direction_ = {0.f, 1.f, 0.f};
};

}  // namespace environment
