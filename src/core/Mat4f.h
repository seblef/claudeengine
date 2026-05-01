#pragma once

#include <cmath>

#if CORE_SIMD_ENABLED
#  include <immintrin.h>
#endif

#include "core/MathUtils.h"
#include "core/Mat3f.h"
#include "core/Vec3f.h"

namespace core {

// Row-major 4x4 float matrix for 3D homogeneous transforms.
//
// All 4 rows are 16-byte aligned (offsets 0, 16, 32, 48 from the base
// pointer) enabling safe _mm_load_ps on every row.  Default-constructed to
// identity.
//
// IMPORTANT: alignas(16) means std::vector and new/delete respect alignment
// from C++17 onward.  Stack allocations are also guaranteed to be aligned.
class alignas(16) Mat4f {
 public:
  // ---- Constants (defined after class — class must be complete) ------------

  static const Mat4f kIdentity;

  // ---- Construction --------------------------------------------------------

  Mat4f() = default;

  // Constructs from 16 elements in row-major order.
  inline Mat4f(float m00, float m01, float m02, float m03,
               float m10, float m11, float m12, float m13,
               float m20, float m21, float m22, float m23,
               float m30, float m31, float m32, float m33) {
    data_[0]=m00;  data_[1]=m01;  data_[2]=m02;  data_[3]=m03;
    data_[4]=m10;  data_[5]=m11;  data_[6]=m12;  data_[7]=m13;
    data_[8]=m20;  data_[9]=m21;  data_[10]=m22; data_[11]=m23;
    data_[12]=m30; data_[13]=m31; data_[14]=m32; data_[15]=m33;
  }

  // ---- Element access ------------------------------------------------------

  // Row and column indices must be in [0, 3].
  [[nodiscard]] inline float  operator()(int row, int col) const { return data_[row * 4 + col]; }
               inline float& operator()(int row, int col)       { return data_[row * 4 + col]; }

  // ---- Assignment ----------------------------------------------------------

  Mat4f& operator=(const Mat4f&) = default;

  // Copies the upper-left 3x3 from mat3.  Row 3 and column 3 become the
  // identity row/column ([0,0,0,1] on the diagonal).
  inline Mat4f& operator=(const Mat3f& mat3) {
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c)
        data_[r*4+c] = mat3(r, c);
      data_[r*4+3] = 0.f;
    }
    data_[12]=0.f; data_[13]=0.f; data_[14]=0.f; data_[15]=1.f;
    return *this;
  }

  // ---- Arithmetic ----------------------------------------------------------

#if CORE_SIMD_ENABLED
  [[nodiscard]] inline Mat4f operator*(const Mat4f& rhs) const {
    __m128 b0 = _mm_load_ps(&rhs.data_[0]);
    __m128 b1 = _mm_load_ps(&rhs.data_[4]);
    __m128 b2 = _mm_load_ps(&rhs.data_[8]);
    __m128 b3 = _mm_load_ps(&rhs.data_[12]);
    Mat4f result;
    for (int i = 0; i < 4; ++i) {
      __m128 a = _mm_load_ps(&data_[i * 4]);
      __m128 c = _mm_add_ps(
        _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(0,0,0,0)), b0),
                   _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(1,1,1,1)), b1)),
        _mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(2,2,2,2)), b2),
                   _mm_mul_ps(_mm_shuffle_ps(a, a, _MM_SHUFFLE(3,3,3,3)), b3)));
      _mm_store_ps(&result.data_[i * 4], c);
    }
    return result;
  }
#else
  [[nodiscard]] inline Mat4f operator*(const Mat4f& rhs) const {
    Mat4f result;
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        float sum = 0.f;
        for (int k = 0; k < 4; ++k)
          sum += data_[i*4+k] * rhs.data_[k*4+j];
        result.data_[i*4+j] = sum;
      }
    }
    return result;
  }
#endif

  inline Mat4f& operator*=(const Mat4f& rhs) { *this = *this * rhs; return *this; }

  // ---- Matrix operations ---------------------------------------------------

  // Returns the inverse.  Behaviour is undefined if the determinant is near zero.
  // Uses Cramer's rule with precomputed 2x2 sub-determinants (Lengyel method).
  [[nodiscard]] inline Mat4f Inverse() const {
    // 2x2 sub-determinants from rows 0,1 (p) and rows 2,3 (q).
    float p0 = data_[0]*data_[5]  - data_[1]*data_[4];
    float p1 = data_[0]*data_[6]  - data_[2]*data_[4];
    float p2 = data_[0]*data_[7]  - data_[3]*data_[4];
    float p3 = data_[1]*data_[6]  - data_[2]*data_[5];
    float p4 = data_[1]*data_[7]  - data_[3]*data_[5];
    float p5 = data_[2]*data_[7]  - data_[3]*data_[6];

    float q0 = data_[8]*data_[13]  - data_[9]*data_[12];
    float q1 = data_[8]*data_[14]  - data_[10]*data_[12];
    float q2 = data_[8]*data_[15]  - data_[11]*data_[12];
    float q3 = data_[9]*data_[14]  - data_[10]*data_[13];
    float q4 = data_[9]*data_[15]  - data_[11]*data_[13];
    float q5 = data_[10]*data_[15] - data_[11]*data_[14];

    float r = 1.f / (p0*q5 - p1*q4 + p2*q3 + p3*q2 - p4*q1 + p5*q0);

    Mat4f inv;
    inv.data_[0]  = ( data_[5]*q5  - data_[6]*q4  + data_[7]*q3)  * r;
    inv.data_[1]  = (-data_[1]*q5  + data_[2]*q4  - data_[3]*q3)  * r;
    inv.data_[2]  = ( data_[13]*p5 - data_[14]*p4 + data_[15]*p3) * r;
    inv.data_[3]  = (-data_[9]*p5  + data_[10]*p4 - data_[11]*p3) * r;

    inv.data_[4]  = (-data_[4]*q5  + data_[6]*q2  - data_[7]*q1)  * r;
    inv.data_[5]  = ( data_[0]*q5  - data_[2]*q2  + data_[3]*q1)  * r;
    inv.data_[6]  = (-data_[12]*p5 + data_[14]*p2 - data_[15]*p1) * r;
    inv.data_[7]  = ( data_[8]*p5  - data_[10]*p2 + data_[11]*p1) * r;

    inv.data_[8]  = ( data_[4]*q4  - data_[5]*q2  + data_[7]*q0)  * r;
    inv.data_[9]  = (-data_[0]*q4  + data_[1]*q2  - data_[3]*q0)  * r;
    inv.data_[10] = ( data_[12]*p4 - data_[13]*p2 + data_[15]*p0) * r;
    inv.data_[11] = (-data_[8]*p4  + data_[9]*p2  - data_[11]*p0) * r;

    inv.data_[12] = (-data_[4]*q3  + data_[5]*q1  - data_[6]*q0)  * r;
    inv.data_[13] = ( data_[0]*q3  - data_[1]*q1  + data_[2]*q0)  * r;
    inv.data_[14] = (-data_[12]*p3 + data_[13]*p1 - data_[14]*p0) * r;
    inv.data_[15] = ( data_[8]*p3  - data_[9]*p1  + data_[10]*p0) * r;
    return inv;
  }

  // Returns the transpose.
  [[nodiscard]] inline Mat4f Transpose() const {
#if CORE_SIMD_ENABLED
    __m128 r0 = _mm_load_ps(&data_[0]);
    __m128 r1 = _mm_load_ps(&data_[4]);
    __m128 r2 = _mm_load_ps(&data_[8]);
    __m128 r3 = _mm_load_ps(&data_[12]);
    _MM_TRANSPOSE4_PS(r0, r1, r2, r3);
    Mat4f result;
    _mm_store_ps(&result.data_[0],  r0);
    _mm_store_ps(&result.data_[4],  r1);
    _mm_store_ps(&result.data_[8],  r2);
    _mm_store_ps(&result.data_[12], r3);
    return result;
#else
    Mat4f result;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        result.data_[i*4+j] = data_[j*4+i];
    return result;
#endif
  }

  // ---- Static factories ----------------------------------------------------

  // 3D translation.
  [[nodiscard]] static inline Mat4f Translation(Vec3f t) {
    return {1.f, 0.f, 0.f, t.x,
            0.f, 1.f, 0.f, t.y,
            0.f, 0.f, 1.f, t.z,
            0.f, 0.f, 0.f, 1.f};
  }

  // Non-uniform 3D scale.
  [[nodiscard]] static inline Mat4f Scale3D(Vec3f scale) {
    return {scale.x, 0.f,     0.f,     0.f,
            0.f,     scale.y, 0.f,     0.f,
            0.f,     0.f,     scale.z, 0.f,
            0.f,     0.f,     0.f,     1.f};
  }

  // Counter-clockwise rotation around the X axis (radians).
  [[nodiscard]] static inline Mat4f RotationX(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    return {1.f, 0.f, 0.f, 0.f,
            0.f,  c,  -s,  0.f,
            0.f,  s,   c,  0.f,
            0.f, 0.f, 0.f, 1.f};
  }

  // Counter-clockwise rotation around the Y axis (radians).
  [[nodiscard]] static inline Mat4f RotationY(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    return { c,  0.f,  s,  0.f,
             0.f, 1.f, 0.f, 0.f,
            -s,  0.f,  c,  0.f,
             0.f, 0.f, 0.f, 1.f};
  }

  // Counter-clockwise rotation around the Z axis (radians).
  [[nodiscard]] static inline Mat4f RotationZ(float angle) {
    float c = std::cos(angle), s = std::sin(angle);
    return { c,  -s,  0.f, 0.f,
             s,   c,  0.f, 0.f,
             0.f, 0.f, 1.f, 0.f,
             0.f, 0.f, 0.f, 1.f};
  }

  // Combined rotation from extrinsic ZYX Euler angles (radians).
  // Equivalent to RotationZ(z) * RotationY(y) * RotationX(x).
  // Applied to a column vector: first X, then Y, then Z rotation.
  [[nodiscard]] static inline Mat4f Rotation(Vec3f euler_angles) {
    return RotationZ(euler_angles.z) * RotationY(euler_angles.y) * RotationX(euler_angles.x);
  }

 private:
  // Row-major: data_[row * 4 + col].
  float data_[16] = {
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f
  };
};

inline const Mat4f Mat4f::kIdentity{};

}  // namespace core
