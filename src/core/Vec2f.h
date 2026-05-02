#pragma once

#include <cmath>

#include "core/MathUtils.h"

namespace core {

// Forward declarations — method bodies live in the respective Mat headers.
class Mat2f;

// 2-component float vector. Uses plain float storage — a 2-lane SSE load
// wastes half the register and adds shuffle overhead for individual vectors.
// The compiler auto-vectorizes bulk loops of Vec2f more effectively without
// interference from manual SIMD.
class Vec2f {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Vec2f kZero;
  static const Vec2f kOne;
  static const Vec2f kAxisX;
  static const Vec2f kAxisY;

  // ---- Construction --------------------------------------------------------

  Vec2f() = default;
  Vec2f(const Vec2f&) = default;
  Vec2f& operator=(const Vec2f&) = default;

  inline explicit Vec2f(float scalar) : x(scalar), y(scalar) {}
  inline Vec2f(float x, float y) : x(x), y(y) {}

  // ---- Components ----------------------------------------------------------

  float x = 0.f;
  float y = 0.f;

  // ---- Arithmetic operators ------------------------------------------------

  [[nodiscard]] inline Vec2f operator+(const Vec2f& rhs) const { return {x+rhs.x, y+rhs.y}; }
  [[nodiscard]] inline Vec2f operator-(const Vec2f& rhs) const { return {x-rhs.x, y-rhs.y}; }
  [[nodiscard]] inline Vec2f operator*(float s)          const { return {x*s, y*s}; }
  [[nodiscard]] inline Vec2f operator/(float s)          const { return {x/s, y/s}; }
  [[nodiscard]] inline Vec2f operator-()                 const { return {-x, -y}; }

  inline Vec2f& operator+=(const Vec2f& rhs) { x+=rhs.x; y+=rhs.y; return *this; }
  inline Vec2f& operator-=(const Vec2f& rhs) { x-=rhs.x; y-=rhs.y; return *this; }
  inline Vec2f& operator*=(float s)          { x*=s; y*=s; return *this; }
  inline Vec2f& operator/=(float s)          { x/=s; y/=s; return *this; }

  [[nodiscard]] inline bool operator==(const Vec2f& rhs) const { return x == rhs.x && y == rhs.y; }
  [[nodiscard]] inline bool operator!=(const Vec2f& rhs) const { return !(*this == rhs); }

  // ---- Vector operations ---------------------------------------------------

  [[nodiscard]] inline float Dot(const Vec2f& rhs) const {
    return x*rhs.x + y*rhs.y;
  }

  // Returns the scalar z-component of the 3D cross product (x, y, 0) × rhs.
  // Positive when rhs is counter-clockwise from this.
  [[nodiscard]] inline float Cross(const Vec2f& rhs) const {
    return x*rhs.y - y*rhs.x;
  }

  [[nodiscard]] inline float LengthSquared() const { return x*x + y*y; }

  [[nodiscard]] inline float Length() const { return std::sqrt(LengthSquared()); }

  // Returns a unit vector. Behaviour is undefined if length is zero.
  [[nodiscard]] inline Vec2f Normalized() const {
    float inv = FastInvSqrt(LengthSquared());
    return {x*inv, y*inv};
  }

  [[nodiscard]] inline float SquaredDistance(const Vec2f& other) const {
    return (*this - other).LengthSquared();
  }

  [[nodiscard]] inline float Distance(const Vec2f& other) const {
    return std::sqrt(SquaredDistance(other));
  }

  // Returns true if this point lies on segment [a, b] within epsilon.
  [[nodiscard]] inline bool IsBetween(const Vec2f& a, const Vec2f& b,
                                      float eps = 1e-5f) const {
    return std::fabsf(Distance(a) + Distance(b) - a.Distance(b)) <= eps;
  }

  [[nodiscard]] inline Vec2f Lerp(const Vec2f& other, float t) const {
    return *this + (other - *this) * t;
  }

  [[nodiscard]] inline Vec2f Scale(const Vec2f& other) const {
    return {x * other.x, y * other.y};
  }

  [[nodiscard]] inline Vec2f InvScale(const Vec2f& other) const {
    return {x / other.x, y / other.y};
  }

  [[nodiscard]] inline Vec2f Inverse() const { return {1.f / x, 1.f / y}; }

  inline Vec2f& InverseInPlace() { *this = Inverse(); return *this; }

  // ---- Matrix multiplication -----------------------------------------------
  // Definitions live in core/Mat2f.h (requires complete Mat2f type).

  // Applies M to this vector (column-vector semantics: result[k] = dot(M_row_k, v)).
  [[nodiscard]] Vec2f operator*(const Mat2f& m) const;
  Vec2f& operator*=(const Mat2f& m);
};

// Constant definitions — class is complete here so Vec2f constructors resolve.
inline const Vec2f Vec2f::kZero {0.f, 0.f};
inline const Vec2f Vec2f::kOne  {1.f, 1.f};
inline const Vec2f Vec2f::kAxisX{1.f, 0.f};
inline const Vec2f Vec2f::kAxisY{0.f, 1.f};

[[nodiscard]] inline Vec2f operator*(float s, const Vec2f& v) { return v * s; }

}  // namespace core
