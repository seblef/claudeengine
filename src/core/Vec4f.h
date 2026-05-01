#pragma once

#include <cmath>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

namespace core {

// 4-component float vector. Uses SSE __m128 storage when CORE_SIMD_ENABLED.
// All methods are inline for maximum compiler inlining at call sites.
class Vec4f {
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

  // Broadcasts scalar to all four components.
  inline explicit Vec4f(float scalar) {
#if CORE_SIMD_ENABLED
    reg_ = _mm_set1_ps(scalar);
#else
    x_ = y_ = z_ = w_ = scalar;
#endif
  }

  inline Vec4f(float x, float y, float z, float w) {
#if CORE_SIMD_ENABLED
    // _mm_set_ps arg order: w, z, y, x → register [x, y, z, w]
    reg_ = _mm_set_ps(w, z, y, x);
#else
    x_ = x; y_ = y; z_ = z; w_ = w;
#endif
  }

#if CORE_SIMD_ENABLED
  inline explicit Vec4f(__m128 reg) : reg_(reg) {}
  [[nodiscard]] inline __m128 Reg() const { return reg_; }
#endif

  // ---- Component access ----------------------------------------------------

#if CORE_SIMD_ENABLED
  [[nodiscard]] inline float X() const { return _mm_cvtss_f32(reg_); }
  [[nodiscard]] inline float Y() const {
    return _mm_cvtss_f32(_mm_shuffle_ps(reg_, reg_, _MM_SHUFFLE(1, 1, 1, 1)));
  }
  [[nodiscard]] inline float Z() const {
    return _mm_cvtss_f32(_mm_shuffle_ps(reg_, reg_, _MM_SHUFFLE(2, 2, 2, 2)));
  }
  [[nodiscard]] inline float W() const {
    return _mm_cvtss_f32(_mm_shuffle_ps(reg_, reg_, _MM_SHUFFLE(3, 3, 3, 3)));
  }
#else
  [[nodiscard]] inline float X() const { return x_; }
  [[nodiscard]] inline float Y() const { return y_; }
  [[nodiscard]] inline float Z() const { return z_; }
  [[nodiscard]] inline float W() const { return w_; }
#endif

  // ---- Arithmetic operators ------------------------------------------------

#if CORE_SIMD_ENABLED
  [[nodiscard]] inline Vec4f operator+(const Vec4f& rhs) const { return Vec4f(_mm_add_ps(reg_, rhs.reg_)); }
  [[nodiscard]] inline Vec4f operator-(const Vec4f& rhs) const { return Vec4f(_mm_sub_ps(reg_, rhs.reg_)); }
  [[nodiscard]] inline Vec4f operator*(float s)          const { return Vec4f(_mm_mul_ps(reg_, _mm_set1_ps(s))); }
  [[nodiscard]] inline Vec4f operator/(float s)          const { return Vec4f(_mm_div_ps(reg_, _mm_set1_ps(s))); }
  [[nodiscard]] inline Vec4f operator-()                 const { return Vec4f(_mm_sub_ps(_mm_setzero_ps(), reg_)); }
#else
  [[nodiscard]] inline Vec4f operator+(const Vec4f& rhs) const { return {x_+rhs.x_, y_+rhs.y_, z_+rhs.z_, w_+rhs.w_}; }
  [[nodiscard]] inline Vec4f operator-(const Vec4f& rhs) const { return {x_-rhs.x_, y_-rhs.y_, z_-rhs.z_, w_-rhs.w_}; }
  [[nodiscard]] inline Vec4f operator*(float s)          const { return {x_*s, y_*s, z_*s, w_*s}; }
  [[nodiscard]] inline Vec4f operator/(float s)          const { return {x_/s, y_/s, z_/s, w_/s}; }
  [[nodiscard]] inline Vec4f operator-()                 const { return {-x_, -y_, -z_, -w_}; }
#endif

  inline Vec4f& operator+=(const Vec4f& rhs) { *this = *this + rhs; return *this; }
  inline Vec4f& operator-=(const Vec4f& rhs) { *this = *this - rhs; return *this; }
  inline Vec4f& operator*=(float s)          { *this = *this * s;   return *this; }
  inline Vec4f& operator/=(float s)          { *this = *this / s;   return *this; }

  [[nodiscard]] inline bool operator==(const Vec4f& rhs) const {
    return X() == rhs.X() && Y() == rhs.Y() && Z() == rhs.Z() && W() == rhs.W();
  }
  [[nodiscard]] inline bool operator!=(const Vec4f& rhs) const { return !(*this == rhs); }

  // ---- Vector operations ---------------------------------------------------

  // Dot product across all four components.
  [[nodiscard]] inline float Dot(const Vec4f& rhs) const {
#if CORE_SIMD_ENABLED
    return _mm_cvtss_f32(_mm_dp_ps(reg_, rhs.reg_, 0xFF));
#else
    return x_*rhs.x_ + y_*rhs.y_ + z_*rhs.z_ + w_*rhs.w_;
#endif
  }

  [[nodiscard]] inline float LengthSquared() const { return Dot(*this); }

  [[nodiscard]] inline float Length() const { return std::sqrt(LengthSquared()); }

  // Returns a unit vector. Behaviour is undefined if length is zero.
  [[nodiscard]] inline Vec4f Normalized() const {
#if CORE_SIMD_ENABLED
    // _mm_dp_ps mask 0xFF: use all 4 lanes, broadcast result to all lanes.
    __m128 len2    = _mm_dp_ps(reg_, reg_, 0xFF);
    __m128 rsqrt   = _mm_rsqrt_ps(len2);
    // One Newton-Raphson step for near-float precision.
    const __m128 half  = _mm_set1_ps(0.5f);
    const __m128 three = _mm_set1_ps(1.5f);
    __m128 r2      = _mm_mul_ps(rsqrt, rsqrt);
    __m128 refined = _mm_mul_ps(rsqrt, _mm_sub_ps(three, _mm_mul_ps(half, _mm_mul_ps(len2, r2))));
    return Vec4f(_mm_mul_ps(reg_, refined));
#else
    return *this * (1.0f / Length());
#endif
  }

 private:
#if CORE_SIMD_ENABLED
  __m128 reg_;
#else
  float x_ = 0.f, y_ = 0.f, z_ = 0.f, w_ = 0.f;
#endif
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
