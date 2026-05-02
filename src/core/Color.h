#pragma once

#include <cmath>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

namespace core {

// RGBA floating-point color.
//
// Components r, g, b, a are public floats in [0, 1] for standard use, but the
// range is not enforced — values outside [0, 1] are valid for HDR workflows.
// Default-constructed color is opaque black {0, 0, 0, 1}.
//
// The class is aligned to 16 bytes so _mm_load_ps(&r) is safe in SIMD builds,
// mirroring the Vec4f layout.
class alignas(16) Color {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Color kBlack;        // {0, 0, 0, 1}
  static const Color kWhite;        // {1, 1, 1, 1}
  static const Color kTransparent;  // {0, 0, 0, 0}

  // ---- Construction --------------------------------------------------------

  Color() = default;
  Color(const Color&) = default;
  Color& operator=(const Color&) = default;

  // Broadcasts scalar to all four components.
  inline explicit Color(float scalar) : r(scalar), g(scalar), b(scalar), a(scalar) {}

  // Constructs from RGBA components. Alpha defaults to 1 (fully opaque).
  inline Color(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}

#if CORE_SIMD_ENABLED
  inline explicit Color(__m128 reg) { _mm_store_ps(&r, reg); }
  [[nodiscard]] inline __m128 Reg() const { return _mm_load_ps(&r); }
#endif

  // ---- Components ----------------------------------------------------------

  float r = 0.f;
  float g = 0.f;
  float b = 0.f;
  float a = 1.f;

  // ---- Arithmetic operators ------------------------------------------------

#if CORE_SIMD_ENABLED
  [[nodiscard]] inline Color operator+(const Color& rhs) const { return Color(_mm_add_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Color operator-(const Color& rhs) const { return Color(_mm_sub_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Color operator*(const Color& rhs) const { return Color(_mm_mul_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Color operator/(const Color& rhs) const { return Color(_mm_div_ps(Reg(), rhs.Reg())); }
  [[nodiscard]] inline Color operator*(float s)          const { return Color(_mm_mul_ps(Reg(), _mm_set1_ps(s))); }
  [[nodiscard]] inline Color operator/(float s)          const { return Color(_mm_div_ps(Reg(), _mm_set1_ps(s))); }
  [[nodiscard]] inline Color operator-()                 const { return Color(_mm_sub_ps(_mm_setzero_ps(), Reg())); }
#else
  [[nodiscard]] inline Color operator+(const Color& rhs) const { return {r+rhs.r, g+rhs.g, b+rhs.b, a+rhs.a}; }
  [[nodiscard]] inline Color operator-(const Color& rhs) const { return {r-rhs.r, g-rhs.g, b-rhs.b, a-rhs.a}; }
  [[nodiscard]] inline Color operator*(const Color& rhs) const { return {r*rhs.r, g*rhs.g, b*rhs.b, a*rhs.a}; }
  [[nodiscard]] inline Color operator/(const Color& rhs) const { return {r/rhs.r, g/rhs.g, b/rhs.b, a/rhs.a}; }
  [[nodiscard]] inline Color operator*(float s)          const { return {r*s, g*s, b*s, a*s}; }
  [[nodiscard]] inline Color operator/(float s)          const { return {r/s, g/s, b/s, a/s}; }
  [[nodiscard]] inline Color operator-()                 const { return {-r, -g, -b, -a}; }
#endif

  inline Color& operator+=(const Color& rhs) { *this = *this + rhs; return *this; }
  inline Color& operator-=(const Color& rhs) { *this = *this - rhs; return *this; }
  inline Color& operator*=(const Color& rhs) { *this = *this * rhs; return *this; }
  inline Color& operator/=(const Color& rhs) { *this = *this / rhs; return *this; }
  inline Color& operator*=(float s)          { *this = *this * s;   return *this; }
  inline Color& operator/=(float s)          { *this = *this / s;   return *this; }

  [[nodiscard]] inline bool operator==(const Color& rhs) const {
    return r == rhs.r && g == rhs.g && b == rhs.b && a == rhs.a;
  }
  [[nodiscard]] inline bool operator!=(const Color& rhs) const { return !(*this == rhs); }

  // ---- Color operations ----------------------------------------------------

  [[nodiscard]] inline Color Lerp(const Color& other, float t) const {
    return *this + (other - *this) * t;
  }
};

// Constant definitions — class is complete here so Color constructors resolve.
inline const Color Color::kBlack      {0.f, 0.f, 0.f, 1.f};
inline const Color Color::kWhite      {1.f, 1.f, 1.f, 1.f};
inline const Color Color::kTransparent{0.f, 0.f, 0.f, 0.f};

[[nodiscard]] inline Color operator*(float s, const Color& c) { return c * s; }

}  // namespace core
