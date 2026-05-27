#pragma once

#include "core/Vec3f.h"
#include "game/ICameraController.h"

namespace terrain { class TerrainData; }

namespace game {

// FPS-style camera controller: mouse look with keyboard movement.
//
// Movement keys: arrow up/down = forward/back, left/right = strafe,
//                A = ascend, Z = descend (free-fly only), Space = jump.
// Mouse: horizontal delta → yaw, vertical delta → pitch (clamped to ±1.5 rad).
//
// When a terrain is bound via SetTerrain(), vertical position is gravity-driven:
// Space triggers a jump impulse and gravity pulls the player back to the ground.
// A/Z free-fly keys are suppressed in terrain mode.
//
// Each call to Update() rebuilds the camera world transform from the current
// yaw, pitch, and position, then calls SetWorldTransform() on the camera.
class FPSCameraController : public ICameraController {
 public:
  FPSCameraController();

  void SetCamera(GameCamera* camera) override;
  void OnEvent(const core::Event& event) override;
  void Update(float dt) override;

  // Sets the initial world-space position. Call before the first Update().
  void SetPosition(core::Vec3f pos);

  // Binds a terrain for automatic height clamping. Pass nullptr for free-fly.
  void SetTerrain(const terrain::TerrainData* terrain);

 private:
  // cppcheck-suppress unusedStructMember
  static constexpr float kPlayerHeight    = 1.8f;    // eye height above ground (m)
  // cppcheck-suppress unusedStructMember
  static constexpr float kMoveSpeed       = 10.f;    // units/s
  // cppcheck-suppress unusedStructMember
  static constexpr float kMouseSensitivity = 0.002f;  // rad/px
  // cppcheck-suppress unusedStructMember
  static constexpr float kJumpSpeed       = 7.f;     // initial vertical velocity (m/s)
  // cppcheck-suppress unusedStructMember
  static constexpr float kGravity         = 18.f;    // downward acceleration (m/s²)

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
  // cppcheck-suppress unusedStructMember
  bool k_jump_    = false;

  // cppcheck-suppress unusedStructMember
  float vel_y_    = 0.f;   // vertical velocity (m/s), terrain mode only
  // cppcheck-suppress unusedStructMember
  bool grounded_  = true;

  // cppcheck-suppress unusedStructMember
  const terrain::TerrainData* terrain_ = nullptr;
};

}  // namespace game
