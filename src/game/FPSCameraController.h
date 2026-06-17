#pragma once

#include <memory>

#include "core/Vec3f.h"
#include "game/ICameraController.h"

namespace physics { class CharacterController; }

namespace game {

// FPS-style camera controller: mouse look with physics-driven movement.
//
// Movement keys: arrow up/down = forward/back, left/right = strafe left/right,
//                Space = jump.
// Mouse: horizontal delta → yaw, vertical delta → pitch (clamped to ±1.5 rad).
// Requires the platform cursor to be captured (SetCursorCapture(true) on
// abstract::Devices) so look-direction updates at window and screen edges.
//
// When PhysicsSystem is initialized, movement is driven by a kinematic capsule
// (CharacterController).  Gravity and collision are handled by Jolt; the camera
// position is the capsule base + an eye-height offset.
//
// If PhysicsSystem is absent (e.g., unit tests), movement falls back to
// free-fly: WASD / arrow keys move freely, A = ascend, Z = descend.
//
// Each call to Update() rebuilds the camera world transform from the current
// yaw, pitch, and position, then calls SetWorldTransform() on the camera.
class FPSCameraController : public ICameraController {
 public:
  FPSCameraController();
  ~FPSCameraController() override;

  void SetCamera(GameCamera* camera) override;
  void OnEvent(const core::Event& event) override;
  void Update(float dt) override;

  // Sets the initial world-space position. Call before the first Update().
  void SetPosition(core::Vec3f pos);

 private:
  // Movement tuning — loaded from config.yaml [player] at construction time.
  // cppcheck-suppress unusedStructMember
  float capsule_radius_;
  // cppcheck-suppress unusedStructMember
  float capsule_half_height_;
  // cppcheck-suppress unusedStructMember
  float eye_offset_;
  // cppcheck-suppress unusedStructMember
  float move_speed_;
  // cppcheck-suppress unusedStructMember
  float mouse_sensitivity_;
  // cppcheck-suppress unusedStructMember
  float jump_speed_;
  // cppcheck-suppress unusedStructMember
  float gravity_;

  // cppcheck-suppress unusedStructMember
  GameCamera* camera_ = nullptr;

  // cppcheck-suppress unusedStructMember
  float yaw_   = 0.f;
  // cppcheck-suppress unusedStructMember
  float pitch_ = 0.f;

  // Used for free-fly fallback when no CharacterController is available.
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
  // cppcheck-suppress unusedStructMember
  bool k_jump_    = false;

  // Vertical velocity (m/s) — used in both physics and free-fly modes.
  // cppcheck-suppress unusedStructMember
  float vel_y_ = 0.f;

  // Physics-driven kinematic capsule; null when PhysicsSystem is absent.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<physics::CharacterController> character_;
};

}  // namespace game
