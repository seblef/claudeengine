#pragma once

#include <cmath>
#include <utility>

#include "core/Vec3f.h"

namespace editor {

// Converts a normalized direction Vec3f to {yaw, pitch} in degrees.
// yaw  = atan2(d.x, d.z), pitch = asin(-d.y), both in [-180, 180].
inline std::pair<float, float> Vec3fToYawPitch(const core::Vec3f& d) {
  constexpr float kRad2Deg = 180.f / static_cast<float>(M_PI);
  const float yaw   = std::atan2(d.x, d.z) * kRad2Deg;
  const float pitch = std::asin(-d.y)       * kRad2Deg;
  return {yaw, pitch};
}

// Converts yaw and pitch (degrees) to a normalized direction Vec3f.
inline core::Vec3f YawPitchToVec3f(float yaw, float pitch) {
  constexpr float kDeg2Rad = static_cast<float>(M_PI) / 180.f;
  const float y = yaw   * kDeg2Rad;
  const float p = pitch * kDeg2Rad;
  const float cos_p = std::cos(p);
  return {cos_p * std::sin(y), -std::sin(p), cos_p * std::cos(y)};
}

// Rounds value to the nearest multiple of snap.
inline float SnapValue(float value, float snap) {
  return std::round(value / snap) * snap;
}

}  // namespace editor
