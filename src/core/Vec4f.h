#pragma once

#include <cmath>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

namespace core {

// 4-component float vector.
//
// Components x, y, z, w are directly accessible as public members.  The class
// is aligned to 16 bytes so _mm_load_ps(&x) is safe in SIMD builds, mirroring
// the Vec3f layout.  Reg() / Vec4f(__m128) bridge between named members and
// SSE registers for internal arithmetic.
class alignas(16) Vec4f {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Vec4f kZero;   // (0, 0, 0, 0)
  static const Vec4f kOne;    // (1, 1, 1, 1)
  static const Vec4f kAxisX;  // (1, 0, 0, 0)
  static const Vec4f kAxisY;  // (0, 1, 0, 0)
  static const Vec4f kAxisZ;  // (0, 0, 1, 0)
  static const Vec4f kAxisW;  // (0, 0, 0, 1)

  // ---- Construction --------------------------------------------------------

  Vec4f() = default;
  Vec4f(const Vec4f&) = default;
  Vec4f& operator=(const Vec4f&) = default;

  // Broadcasts scalar to all four components.
  inline explicit Vec4f(float scalar) : x(scalar), y(scalar), z(scalar), w(scalar) {}

  inline Vec4f(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

#if CORE_SIMD_ENABLED
  // Constructs from a __m128; stores all 4 lanes into x, y, z, w.
  inline explicit Vec4f(__m128 reg) { _mm_store_ps(&x, reg); }

  // Returns the 4 floats as a __m128.
  [[nodiscard]] inline __m128 Reg() const { return _mm_load_ps(&x); }
#endif

  // ---- Components ----------------------------------------------------------

  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
  float w = 0.f;

  // ---- Arithmetic operators ------------------------------------------------

#if CORE_SIMD_ENABLED
  [[nodiscard]] inline Vec4f operator+(const Vec4f& rhs) const { return Vec4f(_mm_add_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Vec4f operator-(const Vec4f& rhs) const { return Vec4f(_mm_sub_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Vec4f operator*(float s)          const { return Vec4f(_mm_mul_ps(Reg(), _mm_set1_ps(s))); }
  [[nodiscard]] inline Vec4f operator/(float s)          const { return Vec4f(_mm_div_ps(Reg(), _mm_set1_ps(s))); }
  [[nodiscard]] inline Vec4f operator-()                 const { return Vec4f(_mm_sub_ps(_mm_setzero_ps(), Reg())); }
#else
  [[nodiscard]] inline Vec4f operator+(const Vec4f& rhs) const { return {x+rhs.x, y+rhs.y, z+rhs.z, w+rhs.w}; }
  [[nodiscard]] inline Vec4f operator-(const Vec4f& rhs) const { return {x-rhs.x, y-rhs.y, z-rhs.z, w-rhs.w}; }
  [[nodiscard]] inline Vec4f operator*(float s)          const { return {x*s, y*s, z*s, w*s}; }
  [[nodiscard]] inline Vec4f operator/(float s)          const { return {x/s, y/s, z/s, w/s}; }
  [[nodiscard]] inline Vec4f operator-()                 const { return {-x, -y, -z, -w}; }
#endif

  inline Vec4f& operator+=(const Vec4f& rhs) { *this = *this + rhs; return *this; }
  inline Vec4f& operator-=(const Vec4f& rhs) { *this = *this - rhs; return *this; }
  inline Vec4f& operator*=(float s)          { *this = *this * s;   return *this; }
  inline Vec4f& operator/=(float s)          { *this = *this / s;   return *this; }

  [[nodiscard]] inline bool operator==(const Vec4f& rhs) const {
    return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
  }
  [[nodiscard]] inline bool operator!=(const Vec4f& rhs) const { return !(*this == rhs); }

  // ---- Vector operations ---------------------------------------------------

  // Dot product across all four components.
  [[nodiscard]] inline float Dot(const Vec4f& rhs) const {
#if CORE_SIMD_ENABLED
    return _mm_cvtss_f32(_mm_dp_ps(Reg(), rhs.Reg(), 0xFF));
#else
    return x*rhs.x + y*rhs.y + z*rhs.z + w*rhs.w;
#endif
  }

  [[nodiscard]] inline float LengthSquared() const { return Dot(*this); }

  [[nodiscard]] inline float Length() const { return std::sqrt(LengthSquared()); }

  // Returns a unit vector. Behaviour is undefined if length is zero.
  [[nodiscard]] inline Vec4f Normalized() const {
#if CORE_SIMD_ENABLED
    // _mm_dp_ps mask 0xFF: use all 4 lanes, broadcast result to all lanes.
    __m128 len2    = _mm_dp_ps(Reg(), Reg(), 0xFF);
    __m128 rsqrt   = _mm_rsqrt_ps(len2);
    // One Newton-Raphson step for near-float precision.
    const __m128 half  = _mm_set1_ps(0.5f);
    const __m128 three = _mm_set1_ps(1.5f);
    __m128 r2      = _mm_mul_ps(rsqrt, rsqrt);
    __m128 refined = _mm_mul_ps(rsqrt, _mm_sub_ps(three, _mm_mul_ps(half, _mm_mul_ps(len2, r2))));
    return Vec4f(_mm_mul_ps(Reg(), refined));
#else
    return *this * (1.0f / Length());
#endif
  }
};

// Constant definitions — class is complete here so Vec4f constructors resolve.
inline const Vec4f Vec4f::kZero {0.f, 0.f, 0.f, 0.f};
inline const Vec4f Vec4f::kOne  {1.f, 1.f, 1.f, 1.f};
inline const Vec4f Vec4f::kAxisX{1.f, 0.f, 0.f, 0.f};
inline const Vec4f Vec4f::kAxisY{0.f, 1.f, 0.f, 0.f};
inline const Vec4f Vec4f::kAxisZ{0.f, 0.f, 1.f, 0.f};
inline const Vec4f Vec4f::kAxisW{0.f, 0.f, 0.f, 1.f};

[[nodiscard]] inline Vec4f operator*(float s, const Vec4f& v) { return v * s; }

}  // namespace core
