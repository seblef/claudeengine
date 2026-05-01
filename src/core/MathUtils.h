#pragma once

#include <cstdint>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

namespace core {

// Math constants.
inline constexpr float kPi      = 3.14159265358979323846f;
inline constexpr float kHalfPi  = kPi * 0.5f;
inline constexpr float kTwoPi   = kPi * 2.0f;
inline constexpr float kDegToRad = kPi / 180.f;
inline constexpr float kRadToDeg = 180.f / kPi;

// Returns 1/sqrt(x) with near-float precision (~23-bit mantissa).
// Uses SIMD _mm_rsqrt_ss + one Newton-Raphson step when available,
// falls back to the Quake III bit-trick on non-SIMD builds.
// Undefined behaviour if x <= 0.
[[nodiscard]] float FastInvSqrt(float x);

// Returns true if |a - b| <= epsilon.
[[nodiscard]] bool NearlyEqual(float a, float b, float epsilon = 1e-5f);

// Clamps x to [min_val, max_val].
[[nodiscard]] float Clamp(float x, float min_val, float max_val);

// Unclamped linear interpolation: (1-t)*a + t*b.
[[nodiscard]] float Lerp(float a, float b, float t);

// Degree/radian conversion.
[[nodiscard]] float ToRadians(float degrees);
[[nodiscard]] float ToDegrees(float radians);

}  // namespace core
