#pragma once

#include "core/Event.h"
#include "core/Vec3f.h"
#include "game/ICameraController.h"

namespace game {

class GameCamera;
class GameObject;

// Third-person spring-arm camera controller with keyboard-driven orbit.
//
// Follows a target GameObject by positioning the camera behind and above it.
// The camera position is spring-smoothed each frame using a frame-rate-independent
// exponential decay. A PhysicsSystem raycast clips the arm against world geometry
// when the physics system is available.
//
// The look direction is NOT smoothed: the camera always points directly at the
// target, slightly above its centre, to avoid disorienting lag.
//
// Orbit controls: hold U to rotate left, hold I to rotate right, press R to
// reset to directly behind the car. The yaw offset is relative to the target's
// heading so the camera tracks turns naturally.
class ChaseCameraController : public ICameraController {
 public:
  ChaseCameraController();

  void SetCamera(GameCamera* camera) override;
  void OnEvent(const core::Event& event) override;
  void Update(float dt) override;

  // Sets the scene object the camera will follow.
  void SetTarget(const GameObject* target);

  // Extra clearance (metres) added behind the target beyond its bounding-box
  // half-diagonal. The effective arm length is arm_length + bbox_half_diag.
  // Default: 10.0.
  void SetArmLength(float m);

  // Extra clearance (metres) added above the target beyond its bounding-box
  // half-height. The effective height offset is arm_height + bbox_height/2.
  // Default: 3.0.
  void SetArmHeight(float m);

  // Controls how quickly the camera catches up to the desired position.
  // Higher values = snappier. Uses 1 - exp(-k * dt). Default: 8.0.
  void SetSpringStiffness(float k);

  // Sets the orbit angular speed in radians per second.
  // Default: 2.094 rad/s (~π*2/3, full orbit in ~3 s).
  void SetOrbitSpeed(float rad_per_sec);

 private:
  // cppcheck-suppress unusedStructMember
  GameCamera*       camera_      = nullptr;
  // cppcheck-suppress unusedStructMember
  const GameObject* target_      = nullptr;
  // cppcheck-suppress unusedStructMember
  core::Vec3f       position_;
  // cppcheck-suppress unusedStructMember
  bool              initialized_ = false;

  // cppcheck-suppress unusedStructMember
  float arm_length_       = 10.f;
  // cppcheck-suppress unusedStructMember
  float arm_height_       =  3.f;
  // cppcheck-suppress unusedStructMember
  float spring_stiffness_ =  8.f;

  // Orbit state.
  // cppcheck-suppress unusedStructMember
  float yaw_offset_        = 0.f;     // radians, relative to target forward
  // cppcheck-suppress unusedStructMember
  float orbit_speed_       = 2.094f;  // rad/s
  // cppcheck-suppress unusedStructMember
  bool  orbit_left_held_   = false;
  // cppcheck-suppress unusedStructMember
  bool  orbit_right_held_  = false;
};

}  // namespace game
