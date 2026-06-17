#pragma once

#include <yaml-cpp/yaml.h>

namespace core {

// Holds player/character movement tuning from the [player] section of config.yaml.
//
// All values have sensible defaults so the section can be omitted entirely.
class PlayerConfig {
 public:
  // Populates fields from the provided YAML node (the "player" subtree).
  void Parse(const YAML::Node& node);

  // ---- Capsule geometry -------------------------------------------------------

  // Radius of the kinematic capsule's hemispheres (metres).
  [[nodiscard]] float GetCapsuleRadius()     const { return capsule_radius_; }

  // Half-height of the capsule's cylinder portion (metres).
  [[nodiscard]] float GetCapsuleHalfHeight() const { return capsule_half_height_; }

  // Eye position above the capsule base (metres). Camera Y = base + this.
  [[nodiscard]] float GetEyeOffset()         const { return eye_offset_; }

  // ---- Movement tuning --------------------------------------------------------

  // Horizontal movement speed (m/s).
  [[nodiscard]] float GetMoveSpeed()         const { return move_speed_; }

  // Mouse look sensitivity (radians per pixel delta).
  [[nodiscard]] float GetMouseSensitivity()  const { return mouse_sensitivity_; }

  // Initial upward velocity when the player jumps (m/s).
  [[nodiscard]] float GetJumpSpeed()         const { return jump_speed_; }

  // Downward acceleration applied while airborne (m/s²).
  [[nodiscard]] float GetGravity()           const { return gravity_; }

 private:
  // cppcheck-suppress unusedStructMember
  float capsule_radius_      = 0.8f;
  // cppcheck-suppress unusedStructMember
  float capsule_half_height_ = 1.8f;
  // cppcheck-suppress unusedStructMember
  float eye_offset_          = 0.3f;
  // cppcheck-suppress unusedStructMember
  float move_speed_          = 10.f;
  // cppcheck-suppress unusedStructMember
  float mouse_sensitivity_   = 0.002f;
  // cppcheck-suppress unusedStructMember
  float jump_speed_          = 7.f;
  // cppcheck-suppress unusedStructMember
  float gravity_             = 9.81f;
};

}  // namespace core
