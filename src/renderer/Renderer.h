#pragma once

#include <memory>

#include "abstract/BlendFactor.h"
#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "renderer/EmissiveFBO.h"
#include "renderer/FoliageRenderer.h"
#include "renderer/TreeRenderer.h"
#include "renderer/GBuffer.h"
#include "renderer/GeometryData.h"
#include "renderer/NoCullingVisibilitySystem.h"
#include "renderer/OctreeVisibilitySystem.h"
#include "renderer/ShadowDebugRenderer.h"
#include "renderer/ShadowRenderer.h"
#include "terrain/TerrainRenderer.h"

namespace environment { class CloudRenderer; }
namespace environment { class SkyRenderer;   }
namespace environment { class WaterRenderer; }
namespace environment { class WindSystem;    }

namespace renderer {

// Selects which G-buffer attachment is blitted to screen in debug mode.
// kNone runs the full deferred pipeline; any other value skips lighting,
// emissive, and composite and blits the chosen RT directly.
enum class DebugMode : int {
  kNone     = 0,
  kAlbedo   = 1,
  kNormal   = 2,
  kSpecular = 3,
  kDepth    = 4,
};

// Singleton renderer. Owns all per-frame constant buffers and render targets.
//
// Constant buffer slots:
//   Slot 1 (RenderableInfos): per-object world matrix.
//   Slot 2 (SceneInfos): per-frame camera matrices, eye position, time, z_near/z_far.
//   Slot 3 (MaterialInfos): per-material colors and shininess.
//   Slot 5 (CSMInfos): cascade VP matrices and split depths for GlobalLight CSM.
//   Slot 7 (WindInfos): wind_xz, wind_strength, wind_time (global; all passes).
//   Slot 8 (SkyInfos): sun direction, time of day, turbidity (sky pass only).
//   Slot 9 (WaterInfos): water level, colours, sun, foam params (global; all passes).
//
// Render targets (created at video resolution, recreated on resize):
//   gbuffer_              — 3-MRT FBO: albedo (RGBA8), normal (RGBA16F), specular (RGBA8)
//                           + shared depth+stencil (DEPTH24_STENCIL8).
//   emissive_fbo_         — HDR RT (RGBA16F) as color + G-buffer depth+stencil as depth.
//   water_scene_color_rt_ — RGBA16F snapshot of HDR RT taken before the water pass.
//   water_depth_copy_rt_  — DEPTH24STENCIL8 snapshot of depth taken before the water pass.
//
// Frame pipeline (Update()):
//   1. Geometry pass  — G-buffer FBO; MeshRenderer fills albedo, normal, specular.
//   2. Lighting pass  — HDR RT (additive blend); LightRenderer shades each light.
//   3. Sky pass       — HDR RT (depth LEQUAL, no blend); SkyRenderer fills background.
//   4. Emissive pass  — HDR RT (additive, depth read-only); emissive/ambient meshes.
//   4b.Water pass     — copy HDR RT + depth; forward SrcAlpha blend into emissive FBO.
//   5. Composite pass — default framebuffer; gamma correction.
//
// Debug mode (SetDebugMode != kNone):
//   After the geometry pass, blits the chosen G-buffer RT to the default framebuffer
//   and returns early — lighting, emissive, and composite are skipped.
//
// Lifecycle: new Renderer(video) → Instance() calls → Shutdown().
class Renderer : public core::Singleton<Renderer> {
 public:
  // Creates constant buffers (slots 1–3) and all render targets.
  // video must outlive this Renderer.
  explicit Renderer(abstract::VideoDevice* video);
  ~Renderer();

  Renderer(const Renderer&)            = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&)                 = delete;
  Renderer& operator=(Renderer&&)      = delete;

  // Initializes both visibility systems for a cube world of the given side length
  // centered at the origin.  Must be called once before AddRenderable.
  void InitVisibilitySystems(float world_size);

  // Clears all renderable registrations from both visibility systems.
  void ClearVisibilitySystems();

  // Registers a renderable with the appropriate visibility system.
  // Always-visible renderables (IsAlwaysVisible()) go to the no-culling system;
  // all others go to the octree for frustum-based culling.
  void AddRenderable(Renderable* r);

  // Removes a renderable from whichever visibility system holds it.
  void RemoveRenderable(Renderable* r);

  // Sets the active camera and fills the scene CB using the current stored time.
  // camera must remain valid until the next SetCamera or Renderer destruction.
  void SetCamera(const core::Camera* camera);

  // Returns the currently active camera, or nullptr if none has been set.
  [[nodiscard]] const core::Camera* GetCamera() const { return camera_; }

  // Runs the full deferred pipeline for one frame.
  // camera must remain valid until the next Update or SetCamera call.
  // When output_fbo is non-null the composite pass blits into it instead of
  // the default framebuffer; the caller owns and manages the FBO lifetime.
  void Update(float time, const core::Camera* camera,
              abstract::RenderTargetGroup* output_fbo = nullptr);

  // Uploads world_matrix into the renderable infos CB (slot 1).
  void SetRenderableInfos(const core::Mat4f& world_matrix);

  // Returns the shared material-infos CB (slot 3). Passed to Material::Set()
  // so each material can fill colors without owning its own CB.
  [[nodiscard]] abstract::ConstantBuffer* GetMaterialInfosCB() const {
    return material_infos_cb_.get();
  }

  // Recreates all render targets at the new resolution (window resize).
  void OnResize(int w, int h);

  // Recreates G-buffer and emissive FBO at the given resolution.
  // Call from the editor viewport when the panel size changes so the renderer
  // runs at viewport-panel resolution rather than window resolution.
  void ResizeTargets(int w, int h);

  // Registers a TerrainRenderer to be called in the geometry pass.
  // Pass nullptr to detach the current terrain. The caller retains ownership.
  // If a WaterRenderer is already registered, its caustic texture is forwarded
  // to the terrain renderer automatically.
  void SetTerrainRenderer(terrain::TerrainRenderer* terrain);

  // Registers a SkyRenderer to be drawn into the emissive FBO before emissive
  // geometry. Pass nullptr to detach. The caller retains ownership.
  void SetSkyRenderer(environment::SkyRenderer* sky) { sky_renderer_ = sky; }

  // Sets the world time of day (hours, 0–24) forwarded to SkyRenderer each frame.
  void SetSkyWorldTime(float t) { sky_world_time_ = t; }

  // Registers a CloudRenderer drawn into the emissive FBO after the sky pass
  // and before the geometry pass. Pass nullptr to detach. The caller retains
  // ownership.
  void SetCloudRenderer(environment::CloudRenderer* clouds) {
    cloud_renderer_ = clouds;
  }

  // Sets the cloud coverage fraction forwarded to CloudRenderer each frame.
  // 0 = clear sky, 0.5 = partly cloudy, 0.9 = overcast.
  void SetCloudDensity(float density) { cloud_density_ = density; }

  // Registers a WindSystem whose data is uploaded into slot 7 each frame.
  // Pass nullptr to clear — the CB is still bound but filled with zeroes.
  // The caller retains ownership.
  void SetWindSystem(environment::WindSystem* wind) { wind_system_ = wind; }

  // Registers a WaterRenderer to be drawn in the forward water pass (after the
  // emissive pass, before composite). Pass nullptr to detach. The caller retains
  // ownership. Immediately syncs the renderer's half-res SSR target to the current
  // render dimensions (render_w_ × render_h_), which may differ from the video
  // device window size in the editor.
  void SetWaterRenderer(environment::WaterRenderer* water);

  // Sets the sky world time forwarded to WaterRenderer for sun direction and
  // sky zenith colour each frame. Must be kept in sync with SetSkyWorldTime().
  void SetWaterSkyWorldTime(float t) { water_sky_world_time_ = t; }

  // Enables foliage rendering. When set, FoliageRenderer::Render() is called
  // in the geometry pass and FoliageRenderer::RenderBillboards() in the emissive
  // pass. Pass false to disable. The FoliageRenderer singleton must be built
  // before enabling; see FoliageRenderer::Build().
  void SetFoliageEnabled(bool enabled) { foliage_enabled_ = enabled; }

  // Enables tree rendering. When set, TreeRenderer::Render() is called in the
  // geometry pass and TreeRenderer::RenderBillboards() in the emissive pass.
  // Pass false to disable. The TreeRenderer singleton must be built before
  // enabling; see TreeRenderer::Build().
  void SetTreeEnabled(bool enabled) { tree_enabled_ = enabled; }

  // Renders terrain patch edges as a flat-colour wireframe overlay into fbo.
  // No-op when no terrain has been registered or Init() has not been called.
  // Must be called after Update() so the scene UBO at slot 2 is bound.
  void RenderTerrainWireframe(const core::Camera& camera,
                               abstract::RenderTargetGroup* fbo);

  // Selects which G-buffer attachment to visualize. kNone restores the full pipeline.
  void SetDebugMode(DebugMode mode) { debug_mode_ = mode; }

  // Advances the shadow-map debug overlay to the next active shadow map.
  // Cycles through: off → GlobalLight cascades → spot maps → omni cube maps → off.
  // Bound to the Tab key in main.cpp.
  void CycleShadowDebug();

  // ---- Render target accessors (for lighting, emissive, and debug passes) --

  [[nodiscard]] GBuffer*     GetGBuffer()     { return &gbuffer_; }
  [[nodiscard]] EmissiveFBO* GetEmissiveFBO() { return &emissive_fbo_; }

  // Convenience forwarder to EmissiveFBO::GetHDRRT().
  [[nodiscard]] abstract::RenderTarget* GetHDRRT() const {
    return emissive_fbo_.GetHDRRT();
  }

 private:
  void FillSceneInfos();
  void FillWindInfos();
  // Derives sun direction and sky zenith colour from water_sky_world_time_ and
  // pushes them to water_renderer_, then uploads the WaterInfos CB (slot 9).
  void UpdateWaterRenderer();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  const core::Camera*    camera_ = nullptr;
  float                  time_   = 0.f;
  int                    render_w_ = 0;
  int                    render_h_ = 0;

  std::unique_ptr<abstract::ConstantBuffer> renderable_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> scene_infos_cb_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ConstantBuffer> material_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> csm_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> wind_infos_cb_;
  // WaterInfos CB — bound globally so terrain shaders can read water_level.
  std::unique_ptr<abstract::ConstantBuffer> water_infos_cb_;

  std::unique_ptr<NoCullingVisibilitySystem> no_culling_system_;
  std::unique_ptr<OctreeVisibilitySystem>    octree_system_;

  // gbuffer_ must be declared before emissive_fbo_ — the emissive FBO borrows
  // gbuffer's depth RT, so it must be destroyed first (reverse declaration order).
  GBuffer     gbuffer_;
  EmissiveFBO emissive_fbo_;

  // Snapshots taken just before the water forward pass.
  // Owned by Renderer; recreated on resize alongside gbuffer_ and emissive_fbo_.
  std::unique_ptr<abstract::RenderTarget> water_scene_color_rt_;
  std::unique_ptr<abstract::RenderTarget> water_depth_copy_rt_;

  DebugMode debug_mode_ = DebugMode::kNone;

  std::unique_ptr<ShadowDebugRenderer> shadow_debug_renderer_;

  // Optional terrain renderer called in the geometry pass. Not owned by Renderer.
  terrain::TerrainRenderer*  terrain_renderer_ = nullptr;
  // Optional sky renderer drawn into the emissive FBO. Not owned by Renderer.
  // cppcheck-suppress unusedStructMember
  environment::SkyRenderer*   sky_renderer_    = nullptr;
  // cppcheck-suppress unusedStructMember
  float                       sky_world_time_  = 12.f;  // hours, 0–24
  // Optional cloud renderer drawn after sky, before geometry. Not owned by Renderer.
  // cppcheck-suppress unusedStructMember
  environment::CloudRenderer* cloud_renderer_  = nullptr;
  // cppcheck-suppress unusedStructMember
  float                       cloud_density_   = 0.f;
  // Optional wind system uploaded into slot 7 each frame. Not owned by Renderer.
  // cppcheck-suppress unusedStructMember
  environment::WindSystem*    wind_system_          = nullptr;
  // Optional water renderer drawn in the geometry pass. Not owned by Renderer.
  // cppcheck-suppress unusedStructMember
  environment::WaterRenderer* water_renderer_       = nullptr;
  // cppcheck-suppress unusedStructMember
  float                       water_sky_world_time_ = 12.f;  // hours, 0–24

  bool foliage_enabled_ = false;
  bool tree_enabled_    = false;

  // Composite pass resources — gamma-correct the HDR RT to the default framebuffer.
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             composite_shader_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             debug_shader_;
  std::unique_ptr<GeometryData> composite_quad_;
};

}  // namespace renderer
