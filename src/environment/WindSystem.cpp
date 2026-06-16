#include "environment/WindSystem.h"

#include <cmath>

#include "environment/EnvironmentDesc.h"

namespace environment {

namespace {
constexpr float kTwoPi = 6.28318530f;
constexpr float kWindFactor = 5.0f;
}  // namespace

WindSystem::WindSystem(const EnvironmentDesc& desc)
    : base_strength_(desc.wind_strength) {
  // Normalise XZ component; Y from the descriptor is ignored at runtime.
  const core::Vec3f xz(desc.wind_direction.x, 0.f, desc.wind_direction.z);
  base_direction_ = xz.Normalized();
  wind_vec_       = base_direction_ * base_strength_;
}

void WindSystem::Update(float dt) {
  phase_     += dt;
  wind_time_ += dt;

  const float gust = base_strength_ * gust_amplitude_
                     * std::sin(phase_ * kTwoPi * gust_frequency_);
  wind_vec_ = base_direction_ * (base_strength_ + gust);

  wind_displacement_.x += wind_vec_.x * dt * kWindFactor;
  wind_displacement_.y += wind_vec_.z * dt * kWindFactor;
}

core::Vec3f WindSystem::GetWindVector() const {
  return {wind_vec_.x, 0.f, wind_vec_.z};
}

float WindSystem::GetWindStrength() const {
  return wind_vec_.Length();
}

void WindSystem::SetBaseDirection(const core::Vec3f& dir) {
  const core::Vec3f xz(dir.x, 0.f, dir.z);
  base_direction_ = xz.Normalized();
}

void WindSystem::SetBaseStrength(float strength) {
  base_strength_ = strength;
}

}  // namespace environment
