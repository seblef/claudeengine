#pragma once

#include <cmath>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

#include "core/MathUtils.h"
#include "core/Vec3f.h"

namespace core {

// Row-major 3x3 float matrix.
//
// Each row occupies 4 floats so rows land on 16-byte boundaries and load
// cleanly into __m128 registers.  The invariant data_[row*4+3] == 0 is
// maintained by all operations and mirrors the Vec3f w=0 convention.
//
// IMPORTANT: alignas(16) means std::vector and new/delete respect alignment
// from C++17 onward.  Stack allocations are also guaranteed to be aligned.
class alignas(16) Mat3f {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Mat3f kIdentity;

  // ---- Construction --------------------------------------------------------

  Mat3f() = default;

  // Constructs from 9 elements in row-major order.
  inline Mat3f(float m00, float m01, float m02,
               float m10, float m11, float m12,
               float m20, float m21, float m22) {
    data_[0]=m00; data_[1]=m01; data_[2]=m02; data_[3]=0.f;
    data_[4]=m10; data_[5]=m11; data_[6]=m12; data_[7]=0.f;
    data_[8]=m20; data_[9]=m21; data_[10]=m22; data_[11]=0.f;
  }

  // ---- Element access ------------------------------------------------------

  // Row and column indices must be in [0, 2].
  [[nodiscard]] inline float  operator()(int row, int col) const { return data_[row * 4 + col]; }
               inline float& operator()(int row, int col)       { return data_[row * 4 + col]; }

  // ---- Assignment ----------------------------------------------------------

  Mat3f& operator=(const Mat3f&) = default;

  // ---- Arithmetic ----------------------------------------------------------

#if CORE_SIMD_ENABLED
  [[nodiscard]] inline Mat3f operator*(const Mat3f& rhs) const {
    __m128 b0 = _mm_load_ps(&rhs.data_[0]);
    __m128 b1 = _mm_load_ps(&rhs.data_[4]);
    __m128 b2 = _mm_load_ps(&rhs.data_[8]);
    Mat3f result;
    for (int i = 0; i < 3; ++i) {
      __m128 a = _mm_load_ps(&data_[i * 4]);
      __m128 c = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0,0,0,0)), b0),
                   _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(1,1,1,1)), b1)),
        _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(2,2,2,2)), b2));
      _mm_store_ps(&result.data_[i * 4], c);
    }
    return result;
  }

  [[nodiscard]] inline Mat3f operator+(const Mat3f& rhs) const {
    Mat3f result;
    _mm_store_ps(&result.data_[0], _mm_add_ps(_mm_load_ps(&data_[0]), _mm_load_ps(&rhs.data_[0])));
    _mm_store_ps(&result.data_[4], _mm_add_ps(_mm_load_ps(&data_[4]), _mm_load_ps(&rhs.data_[4])));
    _mm_store_ps(&result.data_[8], _mm_add_ps(_mm_load_ps(&data_[8]), _mm_load_ps(&rhs.data_[8])));
    return result;
  }

  [[nodiscard]] inline Mat3f operator-(const Mat3f& rhs) const {
    Mat3f result;
    _mm_store_ps(&result.data_[0], _mm_sub_ps(_mm_load_ps(&data_[0]), _mm_load_ps(&rhs.data_[0])));
    _mm_store_ps(&result.data_[4], _mm_sub_ps(_mm_load_ps(&data_[4]), _mm_load_ps(&rhs.data_[4])));
    _mm_store_ps(&result.data_[8], _mm_sub_ps(_mm_load_ps(&data_[8]), _mm_load_ps(&rhs.data_[8])));
    return result;
  }
#else
  [[nodiscard]] inline Mat3f operator*(const Mat3f& rhs) const {
    Mat3f result;
    for (int i = 0; i < 3; ++i) {
      for (int j = 0; j < 3; ++j) {
        float sum = 0.f;
        for (int k = 0; k < 3; ++k)
          sum += data_[i*4+k] * rhs.data_[k*4+j];
        result.data_[i*4+j] = sum;
      }
    }
    return result;
  }

  [[nodiscard]] inline Mat3f operator+(const Mat3f& rhs) const {
    Mat3f result;
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        result.data_[i*4+j] = data_[i*4+j] + rhs.data_[i*4+j];
    return result;
  }

  [[nodiscard]] inline Mat3f operator-(const Mat3f& rhs) const {
    Mat3f result;
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        result.data_[i*4+j] = data_[i*4+j] - rhs.data_[i*4+j];
    return result;
  }
#endif

  inline Mat3f& operator*=(const Mat3f& rhs) { *this = *this * rhs; return *this; }
  inline Mat3f& operator+=(const Mat3f& rhs) { *this = *this + rhs; return *this; }
  inline Mat3f& operator-=(const Mat3f& rhs) { *this = *this - rhs; return *this; }

  // ---- Matrix operations ---------------------------------------------------

  // Returns the inverse.  Behaviour is undefined if the determinant is near zero.
  [[nodiscard]] inline Mat3f Inverse() const {
    float m00=data_[0], m01=data_[1], m02=data_[2];
    float m10=data_[4], m11=data_[5], m12=data_[6];
    float m20=data_[8], m21=data_[9], m22=data_[10];

    float det = m00*(m11*m22 - m12*m21)
              - m01*(m10*m22 - m12*m20)
              + m02*(m10*m21 - m11*m20);
    float r = 1.f / det;

    return {
       (m11*m22 - m12*m21) * r,  -(m01*m22 - m02*m21) * r,   (m01*m12 - m02*m11) * r,
      -(m10*m22 - m12*m20) * r,   (m00*m22 - m02*m20) * r,  -(m00*m12 - m02*m10) * r,
       (m10*m21 - m11*m20) * r,  -(m00*m21 - m01*m20) * r,   (m00*m11 - m01*m10) * r
    };
  }

  // Returns the transpose.
  [[nodiscard]] inline Mat3f Transpose() const {
#if CORE_SIMD_ENABLED
    __m128 r0 = _mm_load_ps(&data_[0]);
    __m128 r1 = _mm_load_ps(&data_[4]);
    __m128 r2 = _mm_load_ps(&data_[8]);
    __m128 r3 = _mm_setzero_ps();
    _MM_TRANSPOSE4_PS(r0, r1, r2, r3);
    // After transpose: r0..r2 hold the 3 rows of the transposed matrix.
    Mat3f result;
    _mm_store_ps(&result.data_[0], r0);
    _mm_store_ps(&result.data_[4], r1);
    _mm_store_ps(&result.data_[8], r2);
    return result;
#else
    return {
      data_[0], data_[4], data_[8],
      data_[1], data_[5], data_[9],
      data_[2], data_[6], data_[10]
    };
#endif
  }

  // ---- Static factories ----------------------------------------------------

  // Non-uniform 3D scale.
  [[nodiscard]] static inline Mat3f Scale3D(Vec3f scale) {
    return {scale.X(), 0.f,       0.f,
            0.f,       scale.Y(), 0.f,
            0.f,       0.f,       scale.Z()};
  }

  // Counter-clockwise rotation around the X axis (radians).
  [[nodiscard]] static inline Mat3f RotationX(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    return {1.f, 0.f,  0.f,
            0.f,  c,   -s,
            0.f,  s,    c};
  }

  // Counter-clockwise rotation around the Y axis (radians).
  [[nodiscard]] static inline Mat3f RotationY(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    return { c,  0.f, s,
             0.f, 1.f, 0.f,
            -s,  0.f, c};
  }

  // Counter-clockwise rotation around the Z axis (radians).
  [[nodiscard]] static inline Mat3f RotationZ(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    return {c, -s,  0.f,
            s,  c,  0.f,
            0.f, 0.f, 1.f};
  }

  // Combined rotation from extrinsic ZYX Euler angles (radians).
  // Equivalent to RotationZ(z) * RotationY(y) * RotationX(x).
  // Applied to a column vector: first X, then Y, then Z rotation.
  [[nodiscard]] static inline Mat3f Rotation(Vec3f euler_angles) {
    return RotationZ(euler_angles.Z()) * RotationY(euler_angles.Y()) * RotationX(euler_angles.X());
  }

 private:
  // Row-major, stride 4.  data_[row*4+3] is padding kept at 0 so each row
  // aligns to 16 bytes and loads as a __m128 (mirroring Vec3f's w=0 layout).
  float data_[12] = {
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f
  };
};

inline const Mat3f Mat3f::kIdentity{};

}  // namespace core
