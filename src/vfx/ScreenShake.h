#pragma once

#include "core/Vec3f.h"
#include "vfx/IVFXEffect.h"

namespace vfx {

// Fire-and-forget screen-shake effect.
//
// Play() computes a shake magnitude that falls off with distance from the
// active camera, then triggers ICameraController::ApplyShake() once; the
// receiving controller owns the actual decay curve applied to its output
// transform. ScreenShake itself only tracks elapsed time so VFXSystem knows
// when this instance can be reaped.
class ScreenShake : public IVFXEffect {
 public:
  ScreenShake() = default;

  void Play(const core::Vec3f& world_pos, const core::Vec3f& direction) override;
  void Update(float dt) override;
  [[nodiscard]] bool IsFinished() const override { return finished_; }

  // Peak translational offset (metres) at 1 metre from the camera; scaled by
  // inverse distance. Default: 0.4.
  void SetBaseMagnitude(float meters) { base_magnitude_ = meters; }

  // Time (seconds) for the shake to decay to zero. Default: 0.5.
  void SetDuration(float seconds) { duration_ = seconds; }

 private:
  // cppcheck-suppress unusedStructMember
  float base_magnitude_ = 0.4f;
  // cppcheck-suppress unusedStructMember
  float duration_       = 0.5f;
  // cppcheck-suppress unusedStructMember
  float elapsed_         = 0.f;
  // cppcheck-suppress unusedStructMember
  bool  finished_         = true;
};

}  // namespace vfx
