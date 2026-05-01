#pragma once

#include <cmath>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

namespace core {

// 3-component float vector.
//
// Internally stores 4 floats (w_=0) so the data fits a __m128 register with
// no lane wasted on address computation. The w_=0 invariant is maintained by
// all constructors and is preserved automatically by SIMD arithmetic since
// every operation inputs vectors with w_=0.
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

  inline explicit Vec3f(float scalar) : x(scalar), y(scalar), z(scalar) {}

  inline Vec3f(float x, float y, float z) : x(x), y(y), z(z) {}

#if CORE_SIMD_ENABLED
  // Constructs from a __m128; w lane is expected to be 0.
  inline explicit Vec3f(__m128 reg) { _mm_store_ps(&x, reg); }

  // Returns the 4 floats (x, y, z, 0) as a __m128.
  [[nodiscard]] inline __m128 Reg() const { return _mm_load_ps(&x); }
#endif

  // ---- Components ----------------------------------------------------------

  // x, y, z are directly accessible.  w_ is SIMD padding and must remain 0.
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;

  // ---- Arithmetic operators ------------------------------------------------

#if CORE_SIMD_ENABLED
  [[nodiscard]] inline Vec3f operator+(const Vec3f& rhs) const { return Vec3f(_mm_add_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Vec3f operator-(const Vec3f& rhs) const { return Vec3f(_mm_sub_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Vec3f operator*(float s)          const { return Vec3f(_mm_mul_ps(Reg(), _mm_set1_ps(s))); }
  [[nodiscard]] inline Vec3f operator/(float s)          const { return Vec3f(_mm_div_ps(Reg(), _mm_set1_ps(s))); }
  [[nodiscard]] inline Vec3f operator-()                 const { return Vec3f(_mm_sub_ps(_mm_setzero_ps(), Reg())); }
#else
  [[nodiscard]] inline Vec3f operator+(const Vec3f& rhs) const { return {x+rhs.x, y+rhs.y, z+rhs.z}; }
  [[nodiscard]] inline Vec3f operator-(const Vec3f& rhs) const { return {x-rhs.x, y-rhs.y, z-rhs.z}; }
  [[nodiscard]] inline Vec3f operator*(float s)          const { return {x*s, y*s, z*s}; }
  [[nodiscard]] inline Vec3f operator/(float s)          const { return {x/s, y/s, z/s}; }
  [[nodiscard]] inline Vec3f operator-()                 const { return {-x, -y, -z}; }
#endif

  inline Vec3f& operator+=(const Vec3f& rhs) { *this = *this + rhs; return *this; }
  inline Vec3f& operator-=(const Vec3f& rhs) { *this = *this - rhs; return *this; }
  inline Vec3f& operator*=(float s)          { *this = *this * s;   return *this; }
  inline Vec3f& operator/=(float s)          { *this = *this / s;   return *this; }

  [[nodiscard]] inline bool operator==(const Vec3f& rhs) const {
    return x == rhs.x && y == rhs.y && z == rhs.z;
  }
  [[nodiscard]] inline bool operator!=(const Vec3f& rhs) const { return !(*this == rhs); }

  // ---- Vector operations ---------------------------------------------------

  [[nodiscard]] inline float Dot(const Vec3f& rhs) const {
#if CORE_SIMD_ENABLED
    // Mask 0x71: use xyz lanes (bits[6:4]=111), store result in lane 0 (bits[3:0]=0001).
    return _mm_cvtss_f32(_mm_dp_ps(Reg(), rhs.Reg(), 0x71));
#else
    return x*rhs.x + y*rhs.y + z*rhs.z;
#endif
  }

  // Returns the cross product. The w_=0 invariant is maintained automatically:
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
      y*rhs.z - z*rhs.y,
      z*rhs.x - x*rhs.z,
      x*rhs.y - y*rhs.x
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
  // SIMD padding: always 0.  Must remain the last declared member so that
  // x, y, z land at offsets 0, 4, 8 from the class base and _mm_load_ps(&x)
  // loads [x, y, z, w_] into the register.
  float w_ = 0.f;
};

// Constant definitions — class is complete here so Vec3f constructors resolve.
inline const Vec3f Vec3f::kZero {0.f, 0.f, 0.f};
inline const Vec3f Vec3f::kOne  {1.f, 1.f, 1.f};
inline const Vec3f Vec3f::kAxisX{1.f, 0.f, 0.f};
inline const Vec3f Vec3f::kAxisY{0.f, 1.f, 0.f};
inline const Vec3f Vec3f::kAxisZ{0.f, 0.f, 1.f};

[[nodiscard]] inline Vec3f operator*(float s, const Vec3f& v) { return v * s; }

}  // namespace core
