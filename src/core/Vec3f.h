#pragma once

#include <cmath>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

namespace core {

// Forward declarations — method bodies live in the respective Mat headers.
class Mat3f;
class Mat4f;

// 3-component float vector.
class Vec3f {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Vec3f kZero;
  static const Vec3f kOne;
  static const Vec3f kAxisX;
  static const Vec3f kAxisY;
  static const Vec3f kAxisZ;

  // ---- Construction --------------------------------------------------------

  Vec3f() = default;
  Vec3f(const Vec3f&) = default;
  Vec3f& operator=(const Vec3f&) = default;

  inline explicit Vec3f(float scalar) : x(scalar), y(scalar), z(scalar) {}

  inline Vec3f(float x, float y, float z) : x(x), y(y), z(z) {}

  // ---- Components ----------------------------------------------------------

  // x, y, z are directly accessible.
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;

  // ---- Arithmetic operators ------------------------------------------------

  [[nodiscard]] inline Vec3f operator+(const Vec3f& rhs) const { return {x+rhs.x, y+rhs.y, z+rhs.z}; }
  [[nodiscard]] inline Vec3f operator-(const Vec3f& rhs) const { return {x-rhs.x, y-rhs.y, z-rhs.z}; }
  [[nodiscard]] inline Vec3f operator*(float s)          const { return {x*s, y*s, z*s}; }
  [[nodiscard]] inline Vec3f operator/(float s)          const { return {x/s, y/s, z/s}; }
  [[nodiscard]] inline Vec3f operator-()                 const { return {-x, -y, -z}; }

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
    return x*rhs.x + y*rhs.y + z*rhs.z;
  }

  // Returns the cross product. The w_=0 invariant is maintained automatically:
  // the w lanes of both inputs are 0, so the w lane of the result is 0*0-0*0=0.
  [[nodiscard]] inline Vec3f Cross(const Vec3f& rhs) const {
    return {
      y*rhs.z - z*rhs.y,
      z*rhs.x - x*rhs.z,
      x*rhs.y - y*rhs.x
    };
  }

  [[nodiscard]] inline float LengthSquared() const { return Dot(*this); }

  [[nodiscard]] inline float Length() const { return std::sqrt(LengthSquared()); }

  [[nodiscard]] inline float SquaredDistance(const Vec3f& other) const {
    return (*this - other).LengthSquared();
  }

  [[nodiscard]] inline float Distance(const Vec3f& other) const {
    return std::sqrt(SquaredDistance(other));
  }

  // Returns true if this point lies on segment [a, b] within epsilon.
  [[nodiscard]] inline bool IsBetween(const Vec3f& a, const Vec3f& b,
                                      float eps = 1e-5f) const {
    return std::fabs(Distance(a) + Distance(b) - a.Distance(b)) <= eps;
  }

  [[nodiscard]] inline Vec3f Lerp(const Vec3f& other, float t) const {
    return *this + (other - *this) * t;
  }

  [[nodiscard]] inline Vec3f Scale(const Vec3f& other) const {
    return {x * other.x, y * other.y, z * other.z};
  }

  // Scalar: avoids 0/0=NaN in the w_ padding lane.
  [[nodiscard]] inline Vec3f InvScale(const Vec3f& other) const {
    return {x / other.x, y / other.y, z / other.z};
  }

  // Scalar: avoids 1/0=inf in the w_ padding lane.
  [[nodiscard]] inline Vec3f Inverse() const {
    return {1.f / x, 1.f / y, 1.f / z};
  }

  inline Vec3f& InverseInPlace() { *this = Inverse(); return *this; }

  // Returns a unit vector. Behaviour is undefined if length is zero.
  [[nodiscard]] inline Vec3f Normalized() const {
    return *this * (1.0f / Length());
  }

  // ---- Matrix multiplication -----------------------------------------------
  // Definitions live in the respective Mat headers (require complete matrix type).

  // Applies M to this vector (column-vector semantics: result[k] = dot(M_row_k, v)).
  [[nodiscard]] Vec3f operator*(const Mat3f& m) const;
  Vec3f& operator*=(const Mat3f& m);

  // Applies M to this vector as a homogeneous point (implicit w=1); translation is included.
  [[nodiscard]] Vec3f operator*(const Mat4f& m) const;
  Vec3f& operator*=(const Mat4f& m);

};

// Constant definitions — class is complete here so Vec3f constructors resolve.
inline const Vec3f Vec3f::kZero {0.f, 0.f, 0.f};
inline const Vec3f Vec3f::kOne  {1.f, 1.f, 1.f};
inline const Vec3f Vec3f::kAxisX{1.f, 0.f, 0.f};
inline const Vec3f Vec3f::kAxisY{0.f, 1.f, 0.f};
inline const Vec3f Vec3f::kAxisZ{0.f, 0.f, 1.f};

[[nodiscard]] inline Vec3f operator*(float s, const Vec3f& v) { return v * s; }

}  // namespace core
