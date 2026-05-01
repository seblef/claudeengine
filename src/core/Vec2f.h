#pragma once

#include <cmath>

#include "core/MathUtils.h"

namespace core {

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

  inline explicit Vec2f(float scalar) : x_(scalar), y_(scalar) {}
  inline Vec2f(float x, float y) : x_(x), y_(y) {}

  // ---- Component access ----------------------------------------------------

  [[nodiscard]] inline float X() const { return x_; }
  [[nodiscard]] inline float Y() const { return y_; }

  // ---- Arithmetic operators ------------------------------------------------

  [[nodiscard]] inline Vec2f operator+(const Vec2f& rhs) const { return {x_+rhs.x_, y_+rhs.y_}; }
  [[nodiscard]] inline Vec2f operator-(const Vec2f& rhs) const { return {x_-rhs.x_, y_-rhs.y_}; }
  [[nodiscard]] inline Vec2f operator*(float s)          const { return {x_*s, y_*s}; }
  [[nodiscard]] inline Vec2f operator/(float s)          const { return {x_/s, y_/s}; }
  [[nodiscard]] inline Vec2f operator-()                 const { return {-x_, -y_}; }

  inline Vec2f& operator+=(const Vec2f& rhs) { x_+=rhs.x_; y_+=rhs.y_; return *this; }
  inline Vec2f& operator-=(const Vec2f& rhs) { x_-=rhs.x_; y_-=rhs.y_; return *this; }
  inline Vec2f& operator*=(float s)          { x_*=s; y_*=s; return *this; }
  inline Vec2f& operator/=(float s)          { x_/=s; y_/=s; return *this; }

  [[nodiscard]] inline bool operator==(const Vec2f& rhs) const { return x_ == rhs.x_ && y_ == rhs.y_; }
  [[nodiscard]] inline bool operator!=(const Vec2f& rhs) const { return !(*this == rhs); }

  // ---- Vector operations ---------------------------------------------------

  [[nodiscard]] inline float Dot(const Vec2f& rhs) const {
    return x_*rhs.x_ + y_*rhs.y_;
  }

  // Returns the scalar z-component of the 3D cross product (x, y, 0) × rhs.
  // Positive when rhs is counter-clockwise from this.
  [[nodiscard]] inline float Cross(const Vec2f& rhs) const {
    return x_*rhs.y_ - y_*rhs.x_;
  }

  [[nodiscard]] inline float LengthSquared() const { return x_*x_ + y_*y_; }

  [[nodiscard]] inline float Length() const { return std::sqrt(LengthSquared()); }

  // Returns a unit vector. Behaviour is undefined if length is zero.
  [[nodiscard]] inline Vec2f Normalized() const {
    float inv = FastInvSqrt(LengthSquared());
    return {x_*inv, y_*inv};
  }

 private:
  float x_ = 0.f;
  float y_ = 0.f;
};

// Constant definitions — class is complete here so Vec2f constructors resolve.
inline const Vec2f Vec2f::kZero {0.f, 0.f};
inline const Vec2f Vec2f::kOne  {1.f, 1.f};
inline const Vec2f Vec2f::kAxisX{1.f, 0.f};
inline const Vec2f Vec2f::kAxisY{0.f, 1.f};

[[nodiscard]] inline Vec2f operator*(float s, const Vec2f& v) { return v * s; }

}  // namespace core
