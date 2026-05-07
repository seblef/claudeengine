#pragma once

#include <memory>

#include "abstract/ConstantBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/Mat4f.h"
#include "core/Singleton.h"

namespace renderer {

// Singleton renderer. Owns the per-frame constant buffers:
//   Slot 1 (RenderableInfos): per-object data — currently the world matrix.
//     Upload via SetRenderableInfos() before each draw call.
//   Slot 2 (SceneInfos): per-frame data — camera matrices, eye position, time.
//     Filled automatically by Update() each frame.
//
// Lifecycle: new Renderer(video) → Instance() calls → Shutdown().
class Renderer : public core::Singleton<Renderer> {
 public:
  // Creates the renderable infos (slot 1) and scene infos (slot 2) constant
  // buffers. video must outlive this Renderer.
  explicit Renderer(abstract::VideoDevice* video);

  Renderer(const Renderer&)            = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&)                 = delete;
  Renderer& operator=(Renderer&&)      = delete;

  // Sets the active camera and fills the scene CB using the current stored time.
  // camera must remain valid until the next SetCamera or Renderer destruction.
  void SetCamera(const core::Camera* camera);

  // Returns the currently active camera, or nullptr if none has been set.
  [[nodiscard]] const core::Camera* GetCamera() const { return camera_; }

  // Binds both CBs, stores time and camera, then fills the scene infos CB.
  // camera must remain valid until the next Update or SetCamera call.
  void Update(float time, const core::Camera* camera);

  // Uploads world_matrix into the renderable infos CB (slot 1).
  // Call once per draw before issuing geometry calls.
  void SetRenderableInfos(const core::Mat4f& world_matrix);

 private:
  void FillSceneInfos();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                    video_;
  const core::Camera*                       camera_ = nullptr;
  float                                     time_   = 0.f;
  std::unique_ptr<abstract::ConstantBuffer> renderable_infos_cb_;
  std::unique_ptr<abstract::ConstantBuffer> scene_infos_cb_;
};

}  // namespace renderer
