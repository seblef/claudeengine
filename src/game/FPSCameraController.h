#pragma once

#include "core/Vec3f.h"
#include "game/ICameraController.h"

namespace game {

// FPS-style camera controller: mouse look with keyboard movement.
//
// Movement keys: arrow up/down = forward/back, left/right = strafe,
//                A = ascend, Z = descend.
// Mouse: horizontal delta → yaw, vertical delta → pitch (clamped to ±1.5 rad).
//
// Each call to Update() rebuilds the camera world transform from the current
// yaw, pitch, and position, then calls SetWorldTransform() on the camera.
class FPSCameraController : public ICameraController {
 public:
  FPSCameraController();

  void SetCamera(GameCamera* camera) override;
  void OnEvent(const core::Event& event) override;
  void Update(float dt) override;

 private:
  // cppcheck-suppress unusedStructMember
  static constexpr float kMoveSpeed        = 10.f;    // units/s
  // cppcheck-suppress unusedStructMember
  static constexpr float kMouseSensitivity = 0.002f;  // rad/px

  // cppcheck-suppress unusedStructMember
  GameCamera* camera_ = nullptr;

  // cppcheck-suppress unusedStructMember
  float yaw_   = 0.f;
  // cppcheck-suppress unusedStructMember
  float pitch_ = 0.f;

  // cppcheck-suppress unusedStructMember
  core::Vec3f position_;

  // cppcheck-suppress unusedStructMember
  float prev_mouse_x_ = -1.f;
  // cppcheck-suppress unusedStructMember
  float prev_mouse_y_ = -1.f;

  // Held-key flags.
  // cppcheck-suppress unusedStructMember
  bool k_forward_ = false;
  // cppcheck-suppress unusedStructMember
  bool k_back_    = false;
  // cppcheck-suppress unusedStructMember
  bool k_left_    = false;
  // cppcheck-suppress unusedStructMember
  bool k_right_   = false;
  // cppcheck-suppress unusedStructMember
  bool k_up_      = false;
  // cppcheck-suppress unusedStructMember
  bool k_down_    = false;
};

}  // namespace game
