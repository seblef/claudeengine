#pragma once

#include "core/Event.h"
#include "core/Vec3f.h"
#include "game/ICameraController.h"

namespace game {

class GameCamera;
class GameObject;

// Third-person spring-arm camera controller.
//
// Follows a target GameObject by positioning the camera behind and above it.
// The camera position is spring-smoothed each frame using a frame-rate-independent
// exponential decay. A PhysicsSystem raycast clips the arm against world geometry
// when the physics system is available.
//
// The look direction is NOT smoothed: the camera always points directly at the
// target, slightly above its centre, to avoid disorienting lag.
//
// OnEvent() is a no-op: this controller has no keyboard or mouse input.
class ChaseCameraController : public ICameraController {
 public:
  ChaseCameraController();

  void SetCamera(GameCamera* camera) override;
  void OnEvent(const core::Event& event) override {}
  void Update(float dt) override;

  // Sets the scene object the camera will follow.
  void SetTarget(const GameObject* target);

  // Distance (metres) from target to camera along the backward axis.
  // Default: 10.0.
  void SetArmLength(float m);

  // Height offset (metres) added above the target position.
  // Default: 3.0.
  void SetArmHeight(float m);

  // Controls how quickly the camera catches up to the desired position.
  // Higher values = snappier. Uses 1 - exp(-k * dt). Default: 8.0.
  void SetSpringStiffness(float k);

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
};

}  // namespace game
