#pragma once

#include "game/IVehicleController.h"

namespace game {

// Keyboard-driven vehicle controller.
//
// Key bindings:
//   W / Up arrow    — throttle 1.0 (held)
//   S / Down arrow  — brake 1.0 (held; throttle takes priority if both held)
//   A / Left arrow  — steer toward -1.0
//   D / Right arrow — steer toward +1.0
//   Space           — handbrake
//
// Steer ramps toward the target at kSteerSpeed rad/s and returns to 0 at
// kSteerReturnSpeed rad/s when the key is released, giving an analog feel
// without analog hardware.  Throttle and brake are binary (0 or 1).
class PlayerVehicleController : public IVehicleController {
 public:
  PlayerVehicleController() = default;

  void OnEvent(const core::Event& event) override;
  void Update(float dt) override;

  [[nodiscard]] float GetThrottle()  const override { return throttle_; }
  [[nodiscard]] float GetSteer()     const override { return steer_; }
  [[nodiscard]] float GetBrake()     const override { return brake_; }
  [[nodiscard]] bool  GetHandbrake() const override { return k_handbrake_; }

 private:
  // Steer ramp rates (units per second).
  // cppcheck-suppress unusedStructMember
  static constexpr float kSteerSpeed       = 2.0f;
  // cppcheck-suppress unusedStructMember
  static constexpr float kSteerReturnSpeed = 3.0f;

  // Held-key flags.
  // cppcheck-suppress unusedStructMember
  bool k_throttle_  = false;
  // cppcheck-suppress unusedStructMember
  bool k_brake_     = false;
  // cppcheck-suppress unusedStructMember
  bool k_steer_left_  = false;
  // cppcheck-suppress unusedStructMember
  bool k_steer_right_ = false;
  // cppcheck-suppress unusedStructMember
  bool k_handbrake_   = false;

  // Derived values — updated each Update().
  // cppcheck-suppress unusedStructMember
  float throttle_ = 0.f;
  // cppcheck-suppress unusedStructMember
  float steer_    = 0.f;
  // cppcheck-suppress unusedStructMember
  float brake_    = 0.f;
};

}  // namespace game
