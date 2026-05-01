#include "core/MathUtils.h"

#include <bit>
#include <cmath>
#include <cstdint>

namespace core {

float FastInvSqrt(float x) {
#if CORE_SIMD_ENABLED
  __m128 val    = _mm_set_ss(x);
  __m128 approx = _mm_rsqrt_ss(val);  // hardware estimate, ~12-bit precision
  float r;
  _mm_store_ss(&r, approx);
  r = r * (1.5f - 0.5f * x * r * r);  // one Newton-Raphson step → ~23-bit
  return r;
#else
  // Quake III Q_rsqrt — type-punned via std::bit_cast (C++20, no UB).
  int32_t i = std::bit_cast<int32_t>(x);
  i         = 0x5f3759df - (i >> 1);
  float r   = std::bit_cast<float>(i);
  r         = r * (1.5f - 0.5f * x * r * r);
  return r;
#endif
}

bool NearlyEqual(float a, float b, float epsilon) {
  float diff = a - b;
  return (diff >= -epsilon) & (diff <= epsilon);
}

float Clamp(float x, float min_val, float max_val) {
  if (x < min_val) return min_val;
  if (x > max_val) return max_val;
  return x;
}

float Lerp(float a, float b, float t) {
  return a + t * (b - a);
}

float ToRadians(float degrees) {
  return degrees * kDegToRad;
}

float ToDegrees(float radians) {
  return radians * kRadToDeg;
}

}  // namespace core
