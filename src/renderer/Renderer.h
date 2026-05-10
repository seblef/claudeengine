#pragma once

#include <memory>

#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"

namespace renderer {

// Singleton renderer. Owns all per-frame constant buffers and render targets.
//
// Constant buffer slots:
//   Slot 1 (RenderableInfos): per-object world matrix.
//   Slot 2 (SceneInfos): per-frame camera matrices, eye position, time.
//   Slot 3 (MaterialInfos): per-material colors and shininess.
//
// Render targets (created at video resolution, recreated on resize):
//   G-buffer — 3-MRT FBO: albedo (RGBA8), normal (RGBA16F), specular (RGBA8)
//              + shared depth+stencil (DEPTH24_STENCIL8).
//   HDR RT   — RGBA16F accumulation target (lighting + emissive).
//   Emissive FBO — HDR RT as color + G-buffer depth+stencil as depth.
//
// Frame pipeline (Update()):
//   1. Geometry pass: bind G-buffer, clear, MeshRenderer fills all 3 MRTs.
//   Lighting, emissive, and composite phases are added in later issues.
//
// Lifecycle: new Renderer(video) → Instance() calls → Shutdown().
class Renderer : public core::Singleton<Renderer> {
 public:
  // Creates constant buffers (slots 1–3) and G-buffer / HDR render targets.
  // video must outlive this Renderer.
  explicit Renderer(abstract::VideoDevice* video);
  ~Renderer();

  Renderer(const Renderer&)            = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&)                 = delete;
  Renderer& operator=(Renderer&&)      = delete;

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

  // ---- Render target accessors (for lighting, emissive, and debug passes) --

  [[nodiscard]] abstract::RenderTargetGroup* GetGBuffer()     const { return gbuffer_.get(); }
  [[nodiscard]] abstract::RenderTarget*      GetHDRRT()       const { return hdr_rt_.get(); }
  [[nodiscard]] abstract::RenderTargetGroup* GetEmissiveFBO() const { return emissive_fbo_.get(); }

 private:
  void FillSceneInfos();
  void CreateRenderTargets(int w, int h);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  const core::Camera*    camera_ = nullptr;
  float                  time_   = 0.f;

  std::unique_ptr<abstract::ConstantBuffer> renderable_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> scene_infos_cb_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ConstantBuffer> material_infos_cb_;

  // Individual RTs — declared before FBO bundles so they outlive them.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget> gbuffer_albedo_rt_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget> gbuffer_normal_rt_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget> gbuffer_specular_rt_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget> gbuffer_depth_rt_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget> hdr_rt_;

  // FBO bundles — declared after RTs so they are destroyed first on cleanup.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTargetGroup> gbuffer_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTargetGroup> emissive_fbo_;
};

}  // namespace renderer
