#pragma once

#include <cmath>

#include "core/MathUtils.h"
#include "core/Vec2f.h"

namespace core {

// Row-major 2x2 float matrix.  Scalar-only: a 4-element footprint makes the
// SSE setup overhead exceed any arithmetic gain.  Default-constructed to
// identity.
class Mat2f {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Mat2f kIdentity;

  // ---- Construction --------------------------------------------------------

  Mat2f() = default;

  // Constructs from 4 elements in row-major order.
  inline Mat2f(float m00, float m01,
               float m10, float m11) {
    data_[0] = m00; data_[1] = m01;
    data_[2] = m10; data_[3] = m11;
  }

  // ---- Element access ------------------------------------------------------

  // Row and column indices must be in [0, 1].
  [[nodiscard]] inline float  operator()(int row, int col) const { return data_[row * 2 + col]; }
               inline float& operator()(int row, int col)       { return data_[row * 2 + col]; }

  // ---- Assignment ----------------------------------------------------------

  Mat2f& operator=(const Mat2f&) = default;

  // ---- Arithmetic ----------------------------------------------------------

  [[nodiscard]] inline Mat2f operator*(const Mat2f& rhs) const {
    return {
      data_[0]*rhs.data_[0] + data_[1]*rhs.data_[2],
      data_[0]*rhs.data_[1] + data_[1]*rhs.data_[3],
      data_[2]*rhs.data_[0] + data_[3]*rhs.data_[2],
      data_[2]*rhs.data_[1] + data_[3]*rhs.data_[3]
    };
  }

  inline Mat2f& operator*=(const Mat2f& rhs) { *this = *this * rhs; return *this; }

  // ---- Matrix operations ---------------------------------------------------

  // Returns the inverse.  Behaviour is undefined if the determinant is zero.
  [[nodiscard]] inline Mat2f Inverse() const {
    float inv_det = 1.f / (data_[0]*data_[3] - data_[1]*data_[2]);
    return { data_[3]*inv_det, -data_[1]*inv_det,
            -data_[2]*inv_det,  data_[0]*inv_det };
  }

  // ---- Static factories ----------------------------------------------------

  // Counter-clockwise rotation by angle (radians).
  [[nodiscard]] static inline Mat2f Rotation2D(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    return {c, -s,
            s,  c};
  }

  // Non-uniform scale.
  [[nodiscard]] static inline Mat2f Scale2D(Vec2f scale) {
    return {scale.X(), 0.f,
            0.f, scale.Y()};
  }

  // Combined scale-after-rotation: M = Scale * Rotation.
  // Applying M to a column vector first rotates then scales it.
  [[nodiscard]] static inline Mat2f RotationScale2D(float angle, Vec2f scale) {
    return Scale2D(scale) * Rotation2D(angle);
  }

 private:
  // Row-major: data_[row * 2 + col].
  float data_[4] = {1.f, 0.f,
                    0.f, 1.f};
};

inline const Mat2f Mat2f::kIdentity{};

}  // namespace core
