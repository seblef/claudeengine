#include "game/CameraShake.h"

#include <algorithm>
#include <cmath>

namespace game {

namespace {
constexpr float kLateralFrequency  = 37.f;   // rad/s
constexpr float kVerticalFrequency = 29.f;   // rad/s
constexpr float kVerticalPhase     = 1.7f;   // rad
constexpr float kRollFrequency     = 41.f;   // rad/s
constexpr float kRollScale         = 0.5f;   // roll radians per magnitude unit
}  // namespace

void CameraShake::Trigger(float magnitude, float duration_seconds) {
  magnitude_ = magnitude;
  duration_  = duration_seconds;
  elapsed_   = 0.f;
}

void CameraShake::Advance(float dt) {
  if (elapsed_ >= duration_) {
    lateral_offset_ = vertical_offset_ = roll_offset_ = 0.f;
    return;
  }
  elapsed_ += dt;
  const float decay  = std::max(0.f, 1.f - elapsed_ / duration_);
  const float amount = magnitude_ * decay;

  lateral_offset_  = std::sin(elapsed_ * kLateralFrequency) * amount;
  vertical_offset_ = std::sin(elapsed_ * kVerticalFrequency + kVerticalPhase) * amount;
  roll_offset_      = std::sin(elapsed_ * kRollFrequency) * amount * kRollScale;
}

}  // namespace game
