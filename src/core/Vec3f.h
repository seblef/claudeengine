#pragma once

#include <cmath>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

namespace core {

// 3-component float vector.
//
// Internally stores 4 floats (w=0) so the data fits a __m128 register with
// no lane wasted on address computation. The w=0 invariant is maintained by
// all constructors and is preserved automatically by SIMD arithmetic since
// every operation inputs vectors with w=0.
//
// IMPORTANT: The class is aligned to 16 bytes so _mm_load_ps (aligned load)
// can be used safely. If you allocate arrays of Vec3f on the heap, use an
// allocator that respects the alignment (e.g. std::vector respects alignas
// since C++17).
class alignas(16) Vec3f {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Vec3f kZero;
  static const Vec3f kOne;
  static const Vec3f kAxisX;
  static const Vec3f kAxisY;
  static const Vec3f kAxisZ;

  // ---- Construction --------------------------------------------------------

  Vec3f() = default;

  inline explicit Vec3f(float scalar) {
    data_[0] = data_[1] = data_[2] = scalar;
    data_[3] = 0.f;
  }

  inline Vec3f(float x, float y, float z) {
    data_[0] = x; data_[1] = y; data_[2] = z; data_[3] = 0.f;
  }

#if CORE_SIMD_ENABLED
  // Constructs from a __m128; w lane is expected to be 0.
  inline explicit Vec3f(__m128 reg) { _mm_store_ps(data_, reg); }

  // Returns the internal 16-byte aligned data as a __m128.
  [[nodiscard]] inline __m128 Reg() const { return _mm_load_ps(data_); }
#endif

  // ---- Component access ----------------------------------------------------

  [[nodiscard]] inline float X() const { return data_[0]; }
  [[nodiscard]] inline float Y() const { return data_[1]; }
  [[nodiscard]] inline float Z() const { return data_[2]; }

  // ---- Arithmetic operators ------------------------------------------------

#if CORE_SIMD_ENABLED
  [[nodiscard]] inline Vec3f operator+(const Vec3f& rhs) const { return Vec3f(_mm_add_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Vec3f operator-(const Vec3f& rhs) const { return Vec3f(_mm_sub_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Vec3f operator*(float s)          const { return Vec3f(_mm_mul_ps(Reg(), _mm_set1_ps(s))); }
  [[nodiscard]] inline Vec3f operator/(float s)          const { return Vec3f(_mm_div_ps(Reg(), _mm_set1_ps(s))); }
  [[nodiscard]] inline Vec3f operator-()                 const { return Vec3f(_mm_sub_ps(_mm_setzero_ps(), Reg())); }
#else
  [[nodiscard]] inline Vec3f operator+(const Vec3f& rhs) const { return {data_[0]+rhs.data_[0], data_[1]+rhs.data_[1], data_[2]+rhs.data_[2]}; }
  [[nodiscard]] inline Vec3f operator-(const Vec3f& rhs) const { return {data_[0]-rhs.data_[0], data_[1]-rhs.data_[1], data_[2]-rhs.data_[2]}; }
  [[nodiscard]] inline Vec3f operator*(float s)          const { return {data_[0]*s, data_[1]*s, data_[2]*s}; }
  [[nodiscard]] inline Vec3f operator/(float s)          const { return {data_[0]/s, data_[1]/s, data_[2]/s}; }
  [[nodiscard]] inline Vec3f operator-()                 const { return {-data_[0], -data_[1], -data_[2]}; }
#endif

  inline Vec3f& operator+=(const Vec3f& rhs) { *this = *this + rhs; return *this; }
  inline Vec3f& operator-=(const Vec3f& rhs) { *this = *this - rhs; return *this; }
  inline Vec3f& operator*=(float s)          { *this = *this * s;   return *this; }
  inline Vec3f& operator/=(float s)          { *this = *this / s;   return *this; }

  [[nodiscard]] inline bool operator==(const Vec3f& rhs) const {
    return data_[0] == rhs.data_[0] && data_[1] == rhs.data_[1] && data_[2] == rhs.data_[2];
  }
  [[nodiscard]] inline bool operator!=(const Vec3f& rhs) const { return !(*this == rhs); }

  // ---- Vector operations ---------------------------------------------------

  [[nodiscard]] inline float Dot(const Vec3f& rhs) const {
#if CORE_SIMD_ENABLED
    // Mask 0x71: use xyz lanes (bits[6:4]=111), store result in lane 0 (bits[3:0]=0001).
    return _mm_cvtss_f32(_mm_dp_ps(Reg(), rhs.Reg(), 0x71));
#else
    return data_[0]*rhs.data_[0] + data_[1]*rhs.data_[1] + data_[2]*rhs.data_[2];
#endif
  }

  // Returns the cross product. The w=0 invariant is maintained automatically:
  // the w lanes of both inputs are 0, so the w lane of the result is 0*0-0*0=0.
  [[nodiscard]] inline Vec3f Cross(const Vec3f& rhs) const {
#if CORE_SIMD_ENABLED
    __m128 a = Reg(), b = rhs.Reg();
    // (y,z,x,0) × (z,x,y,0) - (z,x,y,0) × (y,z,x,0)
    __m128 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_zxy = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 a_zxy = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));
    return Vec3f(_mm_sub_ps(_mm_mul_ps(a_yzx, b_zxy), _mm_mul_ps(a_zxy, b_yzx)));
#else
    return {
      data_[1]*rhs.data_[2] - data_[2]*rhs.data_[1],
      data_[2]*rhs.data_[0] - data_[0]*rhs.data_[2],
      data_[0]*rhs.data_[1] - data_[1]*rhs.data_[0]
    };
#endif
  }

  [[nodiscard]] inline float LengthSquared() const { return Dot(*this); }

  [[nodiscard]] inline float Length() const { return std::sqrt(LengthSquared()); }

  // Returns a unit vector. Behaviour is undefined if length is zero.
  [[nodiscard]] inline Vec3f Normalized() const {
#if CORE_SIMD_ENABLED
    __m128 v = Reg();
    // Mask 0x77: use xyz lanes, broadcast result to xyz lanes (w stays 0).
    __m128 len2    = _mm_dp_ps(v, v, 0x77);
    __m128 rsqrt   = _mm_rsqrt_ps(len2);
    const __m128 half  = _mm_set1_ps(0.5f);
    const __m128 three = _mm_set1_ps(1.5f);
    __m128 r2      = _mm_mul_ps(rsqrt, rsqrt);
    __m128 refined = _mm_mul_ps(rsqrt, _mm_sub_ps(three, _mm_mul_ps(half, _mm_mul_ps(len2, r2))));
    return Vec3f(_mm_mul_ps(v, refined));
#else
    return *this * (1.0f / Length());
#endif
  }

 private:
  // Always 4 floats; data_[3] is padding kept at 0 for __m128 alignment.
  float data_[4] = {0.f, 0.f, 0.f, 0.f};
};

// Constant definitions — class is complete here so Vec3f constructors resolve.
inline const Vec3f Vec3f::kZero {0.f, 0.f, 0.f};
inline const Vec3f Vec3f::kOne  {1.f, 1.f, 1.f};
inline const Vec3f Vec3f::kAxisX{1.f, 0.f, 0.f};
inline const Vec3f Vec3f::kAxisY{0.f, 1.f, 0.f};
inline const Vec3f Vec3f::kAxisZ{0.f, 0.f, 1.f};

[[nodiscard]] inline Vec3f operator*(float s, const Vec3f& v) { return v * s; }

}  // namespace core
