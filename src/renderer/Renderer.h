#pragma once

#include <memory>

#include "abstract/BlendFactor.h"
#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "renderer/EmissiveFBO.h"
#include "renderer/GBuffer.h"
#include "renderer/GeometryData.h"
#include "renderer/NoCullingVisibilitySystem.h"
#include "renderer/OctreeVisibilitySystem.h"

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
//
// Render targets (created at video resolution, recreated on resize):
//   gbuffer_      — 3-MRT FBO: albedo (RGBA8), normal (RGBA16F), specular (RGBA8)
//                   + shared depth+stencil (DEPTH24_STENCIL8).
//   emissive_fbo_ — HDR RT (RGBA16F) as color + G-buffer depth+stencil as depth.
//
// Frame pipeline (Update()):
//   1. Geometry pass  — G-buffer FBO; MeshRenderer fills albedo, normal, specular.
//   2. Lighting pass  — HDR RT (additive blend); LightRenderer shades each light.
//   3. Emissive pass  — HDR RT (additive, depth read-only); emissive/ambient meshes.
//   4. Composite pass — default framebuffer; gamma correction.
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

  // Runs the geometry pass (binds G-buffer, clears, renders meshes) and any
  // subsequent phases that have been wired up. camera must remain valid until
  // the next Update or SetCamera call.
  void Update(float time, const core::Camera* camera);

  // Uploads world_matrix into the renderable infos CB (slot 1).
  void SetRenderableInfos(const core::Mat4f& world_matrix);

  // Returns the shared material-infos CB (slot 3). Passed to Material::Set()
  // so each material can fill colors without owning its own CB.
  [[nodiscard]] abstract::ConstantBuffer* GetMaterialInfosCB() const {
    return material_infos_cb_.get();
  }

  // Recreates all render targets at the new resolution.
  void OnResize(int w, int h);

  // Selects which G-buffer attachment to visualize. kNone restores the full pipeline.
  void SetDebugMode(DebugMode mode) { debug_mode_ = mode; }

  // ---- Render target accessors (for lighting, emissive, and debug passes) --

  [[nodiscard]] GBuffer*     GetGBuffer()     { return &gbuffer_; }
  [[nodiscard]] EmissiveFBO* GetEmissiveFBO() { return &emissive_fbo_; }

  // Convenience forwarder to EmissiveFBO::GetHDRRT().
  [[nodiscard]] abstract::RenderTarget* GetHDRRT() const {
    return emissive_fbo_.GetHDRRT();
  }

 private:
  void FillSceneInfos();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  const core::Camera*    camera_ = nullptr;
  float                  time_   = 0.f;

  std::unique_ptr<abstract::ConstantBuffer> renderable_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> scene_infos_cb_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ConstantBuffer> material_infos_cb_;

  std::unique_ptr<NoCullingVisibilitySystem> no_culling_system_;
  std::unique_ptr<OctreeVisibilitySystem>    octree_system_;

  // gbuffer_ must be declared before emissive_fbo_ — the emissive FBO borrows
  // gbuffer's depth RT, so it must be destroyed first (reverse declaration order).
  GBuffer     gbuffer_;
  EmissiveFBO emissive_fbo_;

  DebugMode debug_mode_ = DebugMode::kNone;

  // Composite pass resources — gamma-correct the HDR RT to the default framebuffer.
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             composite_shader_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             debug_shader_;
  std::unique_ptr<GeometryData> composite_quad_;
};

}  // namespace renderer
