#pragma once

namespace game {

// Decaying sinusoidal shake state shared by ICameraController implementations
// that support ScreenShake (see ICameraController::ApplyShake).
//
// Owns no camera state itself: the controller calls Advance(dt) once per
// frame, then blends GetLateralOffset()/GetVerticalOffset() (metres, along
// its own right/up axes) and GetRollOffset() (radians, around its forward
// axis) into the transform it is about to output.
class CameraShake {
 public:
  CameraShake() = default;

  // (Re)starts the shake: magnitude scales the peak offsets, duration_seconds
  // is the time to linearly decay to zero. Retriggering while already
  // shaking restarts the decay from the new magnitude/duration.
  void Trigger(float magnitude, float duration_seconds);

  // Advances the shake by dt seconds. Call once per frame before reading the
  // offsets below.
  void Advance(float dt);

  [[nodiscard]] float GetLateralOffset()  const { return lateral_offset_; }
  [[nodiscard]] float GetVerticalOffset() const { return vertical_offset_; }
  [[nodiscard]] float GetRollOffset()     const { return roll_offset_; }

 private:
  // cppcheck-suppress unusedStructMember
  float magnitude_ = 0.f;
  // cppcheck-suppress unusedStructMember
  float duration_  = 0.f;
  // cppcheck-suppress unusedStructMember
  float elapsed_   = 0.f;

  // cppcheck-suppress unusedStructMember
  float lateral_offset_  = 0.f;
  // cppcheck-suppress unusedStructMember
  float vertical_offset_ = 0.f;
  // cppcheck-suppress unusedStructMember
  float roll_offset_     = 0.f;
};

}  // namespace game
