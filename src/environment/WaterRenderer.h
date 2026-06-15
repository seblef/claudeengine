#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "abstract/IndexBuffer.h"
#include "abstract/RawTexture.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Singleton.h"
#include "core/Vec2f.h"
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
// Rendering is a two-pass process:
//   Pass 1 — half-resolution SSR: screen-space reflections are rendered into a
//     half-size RGBA16F render target using only the hierarchical ray march.
//     Output is premultiplied alpha (rgb = reflect * weight, a = weight).
//   Pass 2 — full-resolution water: the main water surface is rendered with all
//     visual features.  A cross-bilateral filter guided by the full-res depth
//     buffer upsamples the half-res SSR result to suppress colour bleed across
//     depth discontinuities.
//
// Constant buffer slot 9 (WaterInfos): bound globally by Renderer.
// Sampler slot 0: first water normal map  (RGBA8, tileable; file or flat fallback).
// Sampler slot 1: procedural foam texture (RGBA8, tileable).
// Sampler slot 2: scene_color snapshot (RGBA16F).
// Sampler slot 3: depth snapshot (DEPTH24STENCIL8).
// Sampler slot 4: half-res SSR render target (RGBA16F, premultiplied alpha).
// Sampler slot 5: second water normal map (RGBA8, tileable; file or flat fallback).
//
// The water surface is partitioned into 3 clipmap LOD rings centered on the
// camera and snapped to a coarse grid.  Only ring geometry within the view
// frustum is drawn, and rings are only rebuilt when the camera has moved by at
// least half a cell width since the last rebuild.
//
// Usage:
//   new WaterRenderer();
//   WaterRenderer::Instance().Build(video, water_level);
//   // each frame (pass emissive FBO so Render() can rebind it after the SSR pass):
//   WaterRenderer::Instance().SetSkyZenithColor(r, g, b);
//   WaterRenderer::Instance().SetSunDirection(dir);
//   WaterRenderer::Instance().Render(camera, scene_color_rt, depth_rt, output_fbo);
//   // on viewport resize:
//   WaterRenderer::Instance().Resize(new_w, new_h);
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

  // Creates clipmap LOD ring meshes and loads water shaders.
  // water_level:          world-space Y of the undisplaced water surface.
  // terrain_world_width:  world-space width of the terrain (X axis). Pass 0 for a
  //                       fixed-size mesh centred at the world origin.
  // terrain_world_height: world-space depth of the terrain (Z axis). Pass 0 for a
  //                       fixed-size mesh centred at the world origin.
  // When non-zero terrain dimensions are provided, LOD 2's outer radius extends
  // kTerrainMarginFactor times beyond the terrain's longest half-edge.
  // grid_size: used to derive the LOD 2 outer radius when terrain dimensions are
  //            not supplied (outer = grid_size * 10 m / 2, at least 1000 m).
  // video must outlive this renderer. Screen dimensions are read from video at
  // build time; call Resize() if the viewport changes after Build().
  void Build(abstract::VideoDevice* video, float water_level,
             float terrain_world_width = 0.f, float terrain_world_height = 0.f,
             int grid_size = 128);

  // Executes the two-pass water render: half-res SSR into ssr_rt_, then
  // full-res main water pass into output_fbo.
  // scene_color: HDR snapshot taken just before this call (sampler slot 2).
  // depth:       depth snapshot taken just before this call (sampler slot 3).
  // output_fbo:  the FBO that receives the final water contribution; rebound
  //              internally after the SSR pre-pass.
  // The WaterInfos CB (slot 9) and SceneInfos CB (slot 2) must already be bound.
  void Render(const core::Camera& camera,
              abstract::RenderTarget* scene_color,
              abstract::RenderTarget* depth,
              abstract::RenderTargetGroup* output_fbo);

  // Recreates the half-res SSR render target at the new viewport size.
  // Must be called by the Renderer whenever the screen resolution changes.
  void Resize(int w, int h);

  // Updates the world-space Y of the undisplaced surface (hot-path; no rebuild).
  void SetWaterLevel(float y);

  // Updates the sky zenith colour used for Fresnel reflection.
  void SetSkyZenithColor(float r, float g, float b) {
    sky_zenith_r_ = r;
    sky_zenith_g_ = g;
    sky_zenith_b_ = b;
  }

  // Updates the world-space sun direction for Blinn-Phong specular.
  void SetSunDirection(const core::Vec3f& dir) { sun_direction_ = dir; }

  // Updates the deep water colour.
  void SetWaterColor(float r, float g, float b) {
    water_color_r_ = r;
    water_color_g_ = g;
    water_color_b_ = b;
  }

  // Updates the surface micro-roughness for the specular lobe.
  void SetRoughness(float roughness) { roughness_ = roughness; }

  // Updates the HDR sun power multiplier.
  void SetSunIntensity(float intensity) { sun_intensity_ = intensity; }

  // Updates the UV distortion scale for refraction.
  void SetRefractionStrength(float strength) { refraction_strength_ = strength; }

  // Updates the depth-based colour absorption strength.
  void SetAbsorptionScale(float scale) { absorption_scale_ = scale; }

  // Updates all foam parameters at once.
  void SetFoamParams(float height_thresh, float shoreline_depth,
                     float steepness_thresh, float speed) {
    foam_height_thresh_    = height_thresh;
    foam_shoreline_depth_  = shoreline_depth;
    foam_steepness_thresh_ = steepness_thresh;
    foam_speed_            = speed;
  }

  // Updates scale and scroll speed for both normal map layers.
  void SetNormalMapParams(float scale1, float scale2,
                          float scroll_speed1, float scroll_speed2) {
    normal_scale1_        = scale1;
    normal_scale2_        = scale2;
    normal_scroll_speed1_ = scroll_speed1;
    normal_scroll_speed2_ = scroll_speed2;
  }

  // Loads normal map textures from files (relative to data/textures/).
  // An empty path falls back to a 1×1 flat normal map for that layer.
  // Both textures are re-uploaded to the GPU immediately; safe to call
  // after Build() at any time (e.g. from the editor panel).
  void SetNormalMapTextures(const std::string& path1, const std::string& path2);

  // Updates the XZ distance thresholds used by the water shader LOD system.
  // Fragments closer than lod_near_dist receive full quality (SSR, foam, second
  // normal map, translucency).  Between lod_near_dist and lod_far_dist, SSR is
  // skipped but other features remain active.  Beyond lod_far_dist only the
  // primary normal map and reflections are computed.
  void SetLodNearDist(float d) { lod_near_dist_ = d; }
  void SetLodFarDist(float d)  { lod_far_dist_  = d; }

  // Returns all water parameters packed into WaterInfos for upload by Renderer.
  [[nodiscard]] float GetWaterLevel()   const { return water_level_; }
  [[nodiscard]] float GetSkyZenithR()   const { return sky_zenith_r_; }
  [[nodiscard]] float GetSkyZenithG()   const { return sky_zenith_g_; }
  [[nodiscard]] float GetSkyZenithB()   const { return sky_zenith_b_; }
  [[nodiscard]] const core::Vec3f& GetSunDirection() const { return sun_direction_; }
  [[nodiscard]] float GetWaterColorR()         const { return water_color_r_; }
  [[nodiscard]] float GetWaterColorG()         const { return water_color_g_; }
  [[nodiscard]] float GetWaterColorB()         const { return water_color_b_; }
  [[nodiscard]] float GetRoughness()           const { return roughness_; }
  [[nodiscard]] float GetSunIntensity()        const { return sun_intensity_; }
  [[nodiscard]] float GetRefractionStrength()  const { return refraction_strength_; }
  [[nodiscard]] float GetAbsorptionScale()     const { return absorption_scale_; }
  [[nodiscard]] float GetFoamHeightThresh()    const { return foam_height_thresh_; }
  [[nodiscard]] float GetFoamShorelineDepth()  const { return foam_shoreline_depth_; }
  [[nodiscard]] float GetFoamSteepnessThresh() const { return foam_steepness_thresh_; }
  [[nodiscard]] float GetFoamSpeed()           const { return foam_speed_; }
  [[nodiscard]] float GetNormalScale1()        const { return normal_scale1_; }
  [[nodiscard]] float GetNormalScale2()        const { return normal_scale2_; }
  [[nodiscard]] float GetNormalScrollSpeed1()  const { return normal_scroll_speed1_; }
  [[nodiscard]] float GetNormalScrollSpeed2()  const { return normal_scroll_speed2_; }
  [[nodiscard]] float GetLodNearDist() const { return lod_near_dist_; }
  [[nodiscard]] float GetLodFarDist()  const { return lod_far_dist_;  }

  // Returns the procedural caustic texture, or nullptr if Build() has not been called.
  // The TerrainRenderer binds this at slot 11 to project caustics on submerged terrain.
  [[nodiscard]] abstract::RawTexture* GetCausticTexture() const {
    return caustic_tex_.get();
  }

  // Releases all GPU resources. Called before Shutdown().
  void Reset();

  [[nodiscard]] bool IsReady() const { return shader_ != nullptr; }

 private:
  // Sampler slot used for the half-res SSR render target in the main water pass.
  // cppcheck-suppress unusedStructMember
  static constexpr int kSsrSlot        = 4;
  // Sampler slot used for the second water normal map layer.
  // cppcheck-suppress unusedStructMember
  static constexpr int kNormalMap2Slot  = 5;

  // One tile of a LOD ring, used for per-ring view-frustum culling.
  struct TileInfo {
    // cppcheck-suppress unusedStructMember
    int   first_index;
    // cppcheck-suppress unusedStructMember
    int   index_count;
    // cppcheck-suppress unusedStructMember
    float x0;   // world-space XZ bounds of this tile
    // cppcheck-suppress unusedStructMember
    float z0;
    // cppcheck-suppress unusedStructMember
    float x1;
    // cppcheck-suppress unusedStructMember
    float z1;
  };

  // One clipmap LOD ring.  Geometry is rebuilt (via Fill()) whenever the
  // camera snaps to a new grid-aligned position far enough from last_snap.
  struct LodRing {
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<abstract::VertexBuffer> vb;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<abstract::IndexBuffer>  ib;
    // cppcheck-suppress unusedStructMember
    int           num_indices  = 0;
    // cppcheck-suppress unusedStructMember
    float         cell_size    = 0.f;
    // cppcheck-suppress unusedStructMember
    float         inner_radius = 0.f;
    // cppcheck-suppress unusedStructMember
    float         outer_radius = 0.f;
    // cppcheck-suppress unusedStructMember
    core::Vec2f   last_snap    = {-1e9f, -1e9f};
    // cppcheck-suppress unusedStructMember
    std::vector<TileInfo> tiles;
  };

  // Fraction by which the water plane extends beyond each terrain edge.
  // 1.2 means the water is 20 % wider/deeper than the terrain on every side.
  // cppcheck-suppress unusedStructMember
  static constexpr float kTerrainMarginFactor = 1.2f;

  // Fills ring VB+IB with geometry centered on snap_pos, keeping only quads
  // whose centre falls in [ring.inner_radius, ring.outer_radius] from snap_pos.
  // Indices are emitted in tile-major order for per-tile frustum culling.
  static void BuildRingGeometry(LodRing& ring, core::Vec2f snap_pos);

  // Loads an RGBA8 normal map from a file path relative to data/textures/.
  // Falls back to a 1×1 flat normal map (pointing straight up) when path is empty
  // or the file cannot be decoded.
  [[nodiscard]] std::unique_ptr<abstract::RawTexture> LoadNormalMap(
      const std::string& path);
  void BuildFoamTexture();
  // Generates a 256×256 tileable caustic interference texture from overlapping
  // circular wavefronts.  Result is stored in caustic_tex_.
  void BuildCausticTexture();

  // (Re-)creates ssr_rt_ and ssr_fbo_ at screen_w_/2 × screen_h_/2.
  void BuildSsrTarget();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                  video_      = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                       shader_     = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                       ssr_shader_ = nullptr;
  std::unique_ptr<abstract::RawTexture>   normal_map_tex_;
  std::unique_ptr<abstract::RawTexture>   normal_map_tex2_;
  std::unique_ptr<abstract::RawTexture>   foam_tex_;
  std::unique_ptr<abstract::RawTexture>   caustic_tex_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget>      ssr_rt_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTargetGroup> ssr_fbo_;
  // cppcheck-suppress unusedStructMember
  std::array<LodRing, 3>                  lod_rings_;

  // cppcheck-suppress unusedStructMember
  float         terrain_world_width_  = 0.f;
  // cppcheck-suppress unusedStructMember
  float         terrain_world_height_ = 0.f;
  float         water_level_          = 0.f;
  float         sky_zenith_r_  = 0.40f;
  float         sky_zenith_g_  = 0.65f;
  float         sky_zenith_b_  = 0.90f;
  float         water_color_r_ = 0.02f;
  float         water_color_g_ = 0.08f;
  float         water_color_b_ = 0.15f;
  float         roughness_            = 0.05f;
  float         sun_intensity_        = 20.f;
  float         refraction_strength_  = 0.03f;
  float         absorption_scale_     = 0.20f;
  float         foam_height_thresh_   = 0.60f;
  float         foam_shoreline_depth_ = 2.0f;
  float         foam_steepness_thresh_ = 0.70f;
  float         foam_speed_           = 1.5f;
  float         normal_scale1_        = 0.03f;
  float         normal_scale2_        = 0.07f;
  float         normal_scroll_speed1_ = 0.30f;
  float         normal_scroll_speed2_ = 0.20f;
  // cppcheck-suppress unusedStructMember
  float         lod_near_dist_ = 50.f;
  // cppcheck-suppress unusedStructMember
  float         lod_far_dist_  = 100.f;
  // cppcheck-suppress unusedStructMember
  core::Vec3f   sun_direction_ = {0.f, 1.f, 0.f};
  // cppcheck-suppress unusedStructMember
  int           screen_w_      = 0;
  // cppcheck-suppress unusedStructMember
  int           screen_h_      = 0;
};

}  // namespace environment
