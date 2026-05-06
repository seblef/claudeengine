#pragma once

#include <memory>

#include "abstract/ConstantBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"

namespace renderer {

// Owns and drives the per-frame scene constant buffer (UBO slot 1).
//
// Slot 1 carries a SceneInfos block: all camera-derived matrices, eye position,
// elapsed time, and inverse screen size. Call Update() each frame after
// camera.UpdateMatrices() and before any draw calls.
class Renderer {
 public:
  // Creates and binds the scene constant buffer at slot 1.
  // video must outlive this Renderer.
  explicit Renderer(abstract::VideoDevice* video);

  Renderer(const Renderer&)            = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&)                 = delete;
  Renderer& operator=(Renderer&&)      = delete;

  // Sets the active camera and fills the CB using the current stored time.
  // camera must remain valid until the next SetCamera or Renderer destruction.
  void SetCamera(const core::Camera* camera);

  // Returns the currently active camera, or nullptr if none has been set.
  [[nodiscard]] const core::Camera* GetCamera() const { return camera_; }

  // Stores time and camera, then fills the scene constant buffer for this frame.
  // camera must remain valid until the next Update or SetCamera call.
  void Update(float time, const core::Camera* camera);

 private:
  void FillSceneInfos();

  abstract::VideoDevice*                    video_;
  const core::Camera*                       camera_ = nullptr;
  float                                     time_   = 0.f;
  std::unique_ptr<abstract::ConstantBuffer> scene_infos_cb_;
};

}  // namespace renderer
