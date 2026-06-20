#pragma once

#include <memory>
#include <vector>

#include "abstract/BlendFactor.h"
#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "renderer/BloomRenderer.h"
#include "renderer/EmissiveFBO.h"
#include "renderer/EyeAdaptationRenderer.h"
#include "renderer/PostProcessInfos.h"
#include "renderer/FoliageRenderer.h"
#include "renderer/TreeRenderer.h"
#include "renderer/GBuffer.h"
#include "renderer/GeometryData.h"
#include "renderer/NoCullingVisibilitySystem.h"
#include "renderer/OctreeVisibilitySystem.h"
#include "renderer/ShadowDebugRenderer.h"
#include "renderer/ShadowRenderer.h"
#include "renderer/WireframeRenderer.h"
#include "terrain/TerrainRenderer.h"

namespace environment { class CloudRenderer;       }
namespace environment { class CloudShadowRenderer; }
namespace environment { class SkyRenderer;         }
namespace environment { class WaterRenderer;       }
namespace environment { class WindSystem;          }
namespace particles   { class ParticleRenderer; }
namespace ui          { class UIRenderer; }
namespace ui          { class UISprite;   }
namespace ui          { class UIText;     }

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
//   Slot  1 (RenderableInfos):  per-object world matrix.
//   Slot  2 (SceneInfos):       per-frame camera matrices, eye position, time, z_near/z_far.
//   Slot  3 (MaterialInfos):    per-material colors and shininess.
//   Slot  5 (CSMInfos):         cascade VP matrices and split depths for GlobalLight CSM.
//   Slot  7 (WindInfos):        wind_xz, wind_strength, wind_time (global; all passes).
//   Slot  8 (SkyInfos):         sun direction, time of day, turbidity (sky pass only).
//   Slot  9 (WaterInfos):       water level, colours, sun, foam params (global; all passes).
//   Slot 10 (PostProcessInfos): exposure, bloom_intensity, bloom_threshold, adapt_speed.
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
//                       Must run before the particle forward pass: forward particles do
//                       not write depth, so running water after them would make the
//                       water depth test pass at particle pixels and paint water over them.
//   4c.Particle fwd   — HDR RT; kAdditive (ONE+ONE) and kAlphaBlend (SrcAlpha) emitters.
//                       Runs after water so above-water particles appear on top.
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

  // Registers a CloudShadowRenderer that bakes the cloud density into a shadow
  // texture each frame before the lighting pass.  The shadow texture is bound at
  // sampler slot 13 during the GlobalLight draw.  Pass nullptr to detach.
  // The caller retains ownership.
  void SetCloudShadowRenderer(environment::CloudShadowRenderer* shadow) {
    cloud_shadow_renderer_ = shadow;
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

  // Registers a ParticleRenderer. kGBuffer emitters are drawn at the end of the
  // geometry pass; kAdditive and kAlphaBlend emitters in the forward pass after
  // the emissive pass. Pass nullptr to detach. The caller retains ownership.
  void SetParticleRenderer(particles::ParticleRenderer* p) {
    particle_renderer_ = p;
  }

  // Returns the active ParticleRenderer, or nullptr if none has been set.
  // Used by ParticleRenderable::Enqueue() to forward emitters into the
  // per-frame render queue.
  [[nodiscard]] particles::ParticleRenderer* GetParticleRenderer() const {
    return particle_renderer_;
  }

  // Registers a UIRenderer to be called after the composite pass.
  // Pass nullptr to detach. The caller retains ownership.
  void SetUIRenderer(ui::UIRenderer* ui) { ui_renderer_ = ui; }

  // Adds a UISprite to the per-frame overlay list.
  // The caller retains ownership; the pointer must remain valid each frame.
  void AddUISprite(ui::UISprite* s) { ui_sprites_.push_back(s); }

  // Removes a UISprite from the per-frame overlay list.
  void RemoveUISprite(ui::UISprite* s);

  // Adds a UIText to the per-frame overlay list.
  // The caller retains ownership; the pointer must remain valid each frame.
  void AddUIText(ui::UIText* t) { ui_texts_.push_back(t); }

  // Removes a UIText from the per-frame overlay list.
  void RemoveUIText(ui::UIText* t);

  // Forwarding wrappers for WireframeRenderer configuration.
  static void SetGizmosEnabled(bool enabled) {
    WireframeRenderer::Instance().SetGizmosEnabled(enabled);
  }
  static void SetTerrainWireframeEnabled(bool enabled) {
    WireframeRenderer::Instance().SetTerrainWireframeEnabled(enabled);
  }
  static void SetHighlightedObject(const void* key) {
    WireframeRenderer::Instance().SetHighlightedObject(key);
  }

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

  // Returns a mutable reference to the post-process parameters.
  // The editor writes directly to this struct; the UBO is uploaded each frame
  // before the composite pass so changes take effect on the next frame.
  [[nodiscard]] PostProcessInfos& GetPostProcessInfos() {
    return post_process_infos_;
  }

 private:
  void FillSceneInfos();
  void FillWindInfos();
  // Derives sun direction and sky zenith colour from water_sky_world_time_ and
  // pushes them to water_renderer_, then uploads the WaterInfos CB (slot 9).
  void UpdateWaterRenderer();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  const core::Camera*    camera_    = nullptr;
  float                  time_      = 0.f;
  float                  prev_time_ = 0.f;
  int                    render_w_  = 0;
  int                    render_h_  = 0;

  std::unique_ptr<abstract::ConstantBuffer> renderable_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> scene_infos_cb_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ConstantBuffer> material_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> csm_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> wind_infos_cb_;
  // WaterInfos CB — bound globally so terrain shaders can read water_level.
  std::unique_ptr<abstract::ConstantBuffer> water_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> post_process_infos_cb_;

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
  environment::CloudRenderer*       cloud_renderer_        = nullptr;
  // Optional cloud shadow renderer; bakes shadow texture before lighting pass.
  // Not owned by Renderer.
  // cppcheck-suppress unusedStructMember
  environment::CloudShadowRenderer* cloud_shadow_renderer_ = nullptr;
  // cppcheck-suppress unusedStructMember
  float                             cloud_density_         = 0.f;
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

  // Optional particle renderer for kGBuffer emitters. Not owned by Renderer.
  // cppcheck-suppress unusedStructMember
  particles::ParticleRenderer* particle_renderer_ = nullptr;

  // Optional UI overlay renderer called after the composite pass. Not owned by Renderer.
  // cppcheck-suppress unusedStructMember
  ui::UIRenderer* ui_renderer_ = nullptr;
  // cppcheck-suppress unusedStructMember
  std::vector<ui::UISprite*> ui_sprites_;
  // cppcheck-suppress unusedStructMember
  std::vector<ui::UIText*>   ui_texts_;

  // cppcheck-suppress unusedStructMember
  PostProcessInfos post_process_infos_;

  // Conditionally allocated from PostProcessConfig flags.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<BloomRenderer>          bloom_renderer_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EyeAdaptationRenderer>  eye_adapt_renderer_;

  // 1×1 black R11F_G11F_B10F RT bound at sampler slot 11 when bloom is disabled,
  // keeping the composite shader uniform with or without bloom.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget> null_bloom_rt_;

  // Composite pass resources — tone-map and gamma-correct the HDR RT to the default framebuffer.
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             composite_shader_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             debug_shader_;
  std::unique_ptr<GeometryData> composite_quad_;
};

}  // namespace renderer
