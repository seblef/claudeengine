#include "core/Mat4f.h"
#include "core/MathUtils.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace core;

namespace {

bool Near(float a, float b, float eps = 1e-5f) { return NearlyEqual(a, b, eps); }

void ExpectIdentity4(const Mat4f& m, float eps = 1e-5f) {
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      EXPECT_TRUE(Near(m(r,c), r == c ? 1.f : 0.f, eps));
}

}  // namespace

// ---- Identity ---------------------------------------------------------------

TEST(Mat4fTest, DefaultIsIdentity) {
  ExpectIdentity4(Mat4f{});
}

TEST(Mat4fTest, kIdentityIsIdentity) {
  ExpectIdentity4(Mat4f::kIdentity);
}

// ---- Element access ---------------------------------------------------------

TEST(Mat4fTest, SetAndGetElement) {
  Mat4f m;
  m(2, 3) = 5.f;
  EXPECT_TRUE(Near(m(2, 3), 5.f));
  EXPECT_TRUE(Near(m(2, 2), 1.f));  // rest unchanged
}

// ---- Multiply ---------------------------------------------------------------

TEST(Mat4fTest, MultiplyByIdentityIsNoOp) {
  Mat4f a{1.f,2.f,3.f,4.f, 5.f,6.f,7.f,8.f,
          9.f,10.f,11.f,12.f, 13.f,14.f,15.f,16.f};
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c) {
      EXPECT_TRUE(Near((a * Mat4f::kIdentity)(r,c), a(r,c)));
      EXPECT_TRUE(Near((Mat4f::kIdentity * a)(r,c), a(r,c)));
    }
}

TEST(Mat4fTest, MultiplyIsAssociative) {
  Mat4f a = Mat4f::Translation({1.f, 0.f, 0.f});
  Mat4f b = Mat4f::RotationZ(kPi / 4.f);
  Mat4f c = Mat4f::Scale3D({2.f, 2.f, 2.f});
  Mat4f ab_c = (a * b) * c;
  Mat4f a_bc = a * (b * c);
  for (int r = 0; r < 4; ++r)
    for (int col = 0; col < 4; ++col)
      EXPECT_TRUE(Near(ab_c(r,col), a_bc(r,col), 1e-5f));
}

TEST(Mat4fTest, MultiplyAssignIsConsistent) {
  Mat4f a = Mat4f::RotationX(kPi / 6.f);
  Mat4f b = Mat4f::Scale3D({2.f, 3.f, 4.f});
  Mat4f expected = a * b;
  a *= b;
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      EXPECT_TRUE(Near(a(r,c), expected(r,c)));
}

// ---- Assign from Mat3f ------------------------------------------------------

TEST(Mat4fTest, AssignFromMat3fCopiesToUpperLeft) {
  Mat3f m3{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat4f m4;
  m4 = m3;
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(m4(r,c), m3(r,c)));
  // Column 3 of rows 0-2 must be 0.
  EXPECT_TRUE(Near(m4(0,3), 0.f));
  EXPECT_TRUE(Near(m4(1,3), 0.f));
  EXPECT_TRUE(Near(m4(2,3), 0.f));
  // Row 3 must be homogeneous [0,0,0,1].
  EXPECT_TRUE(Near(m4(3,0), 0.f));
  EXPECT_TRUE(Near(m4(3,1), 0.f));
  EXPECT_TRUE(Near(m4(3,2), 0.f));
  EXPECT_TRUE(Near(m4(3,3), 1.f));
}

// ---- Inverse ----------------------------------------------------------------

TEST(Mat4fTest, InverseOfIdentityIsIdentity) {
  ExpectIdentity4(Mat4f::kIdentity.Inverse());
}

TEST(Mat4fTest, TranslationInverseIsNegation) {
  // Translation({1,2,3})^-1 = Translation({-1,-2,-3})
  Mat4f t    = Mat4f::Translation({1.f, 2.f, 3.f});
  Mat4f t_inv = t.Inverse();
  EXPECT_TRUE(Near(t_inv(0,3), -1.f));
  EXPECT_TRUE(Near(t_inv(1,3), -2.f));
  EXPECT_TRUE(Near(t_inv(2,3), -3.f));
  EXPECT_TRUE(Near(t_inv(3,3),  1.f));
}

TEST(Mat4fTest, MultiplyByInverseIsIdentity) {
  // Use a rotation + scale matrix (well-conditioned, det=1).
  Mat4f m = Mat4f::RotationZ(kPi / 5.f) * Mat4f::Scale3D({2.f, 3.f, 4.f});
  ExpectIdentity4(m * m.Inverse(), 1e-4f);
}

// ---- Transpose --------------------------------------------------------------

TEST(Mat4fTest, TransposeSwapsOffDiagonal) {
  Mat4f a{1.f,2.f,3.f,4.f, 5.f,6.f,7.f,8.f,
          9.f,10.f,11.f,12.f, 13.f,14.f,15.f,16.f};
  Mat4f t = a.Transpose();
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      EXPECT_TRUE(Near(t(r,c), a(c,r)));
}

TEST(Mat4fTest, TransposeOfTransposeIsOriginal) {
  Mat4f a{1.f,2.f,3.f,4.f, 5.f,6.f,7.f,8.f,
          9.f,10.f,11.f,12.f, 13.f,14.f,15.f,16.f};
  Mat4f result = a.Transpose().Transpose();
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      EXPECT_TRUE(Near(result(r,c), a(r,c)));
}

// ---- Factories --------------------------------------------------------------

TEST(Mat4fTest, TranslationMatrix) {
  Mat4f t = Mat4f::Translation({1.f, 2.f, 3.f});
  // Diagonal is identity.
  EXPECT_TRUE(Near(t(0,0), 1.f)); EXPECT_TRUE(Near(t(1,1), 1.f));
  EXPECT_TRUE(Near(t(2,2), 1.f)); EXPECT_TRUE(Near(t(3,3), 1.f));
  // Last column encodes translation.
  EXPECT_TRUE(Near(t(0,3), 1.f));
  EXPECT_TRUE(Near(t(1,3), 2.f));
  EXPECT_TRUE(Near(t(2,3), 3.f));
  EXPECT_TRUE(Near(t(3,3), 1.f));
  // Row 3 is homogeneous.
  EXPECT_TRUE(Near(t(3,0), 0.f)); EXPECT_TRUE(Near(t(3,1), 0.f)); EXPECT_TRUE(Near(t(3,2), 0.f));
}

TEST(Mat4fTest, Scale3DMatrix) {
  Mat4f s = Mat4f::Scale3D({2.f, 3.f, 4.f});
  EXPECT_TRUE(Near(s(0,0), 2.f)); EXPECT_TRUE(Near(s(1,1), 3.f));
  EXPECT_TRUE(Near(s(2,2), 4.f)); EXPECT_TRUE(Near(s(3,3), 1.f));
  // Off-diagonal must be 0.
  EXPECT_TRUE(Near(s(0,1), 0.f)); EXPECT_TRUE(Near(s(0,2), 0.f)); EXPECT_TRUE(Near(s(0,3), 0.f));
}

TEST(Mat4fTest, RotationXHalfPi) {
  // Rx(π/2) = [1,0,0,0; 0,0,-1,0; 0,1,0,0; 0,0,0,1]
  Mat4f r = Mat4f::RotationX(kHalfPi);
  EXPECT_TRUE(Near(r(0,0), 1.f));
  EXPECT_TRUE(Near(r(1,1), 0.f, 1e-5f)); EXPECT_TRUE(Near(r(1,2), -1.f, 1e-5f));
  EXPECT_TRUE(Near(r(2,1), 1.f, 1e-5f)); EXPECT_TRUE(Near(r(2,2),  0.f, 1e-5f));
  EXPECT_TRUE(Near(r(3,3), 1.f));
  EXPECT_TRUE(Near(r(0,3), 0.f)); EXPECT_TRUE(Near(r(1,3), 0.f)); EXPECT_TRUE(Near(r(2,3), 0.f));
}

TEST(Mat4fTest, RotationYHalfPi) {
  // Ry(π/2) = [0,0,1,0; 0,1,0,0; -1,0,0,0; 0,0,0,1]
  Mat4f r = Mat4f::RotationY(kHalfPi);
  EXPECT_TRUE(Near(r(0,0), 0.f, 1e-5f)); EXPECT_TRUE(Near(r(0,2),  1.f, 1e-5f));
  EXPECT_TRUE(Near(r(1,1), 1.f));
  EXPECT_TRUE(Near(r(2,0), -1.f, 1e-5f)); EXPECT_TRUE(Near(r(2,2), 0.f, 1e-5f));
  EXPECT_TRUE(Near(r(3,3), 1.f));
}

TEST(Mat4fTest, RotationZHalfPi) {
  // Rz(π/2) = [0,-1,0,0; 1,0,0,0; 0,0,1,0; 0,0,0,1]
  Mat4f r = Mat4f::RotationZ(kHalfPi);
  EXPECT_TRUE(Near(r(0,0), 0.f, 1e-5f)); EXPECT_TRUE(Near(r(0,1), -1.f, 1e-5f));
  EXPECT_TRUE(Near(r(1,0), 1.f, 1e-5f)); EXPECT_TRUE(Near(r(1,1),  0.f, 1e-5f));
  EXPECT_TRUE(Near(r(2,2), 1.f)); EXPECT_TRUE(Near(r(3,3), 1.f));
}

TEST(Mat4fTest, RotationZeroIsIdentity) {
  ExpectIdentity4(Mat4f::Rotation({0.f, 0.f, 0.f}));
}

TEST(Mat4fTest, RotationXAxisMatchesRotationX) {
  float angle = kPi / 3.f;
  Mat4f a = Mat4f::Rotation({angle, 0.f, 0.f});
  Mat4f b = Mat4f::RotationX(angle);
  for (int r = 0; r < 4; ++r)
    for (int c = 0; c < 4; ++c)
      EXPECT_TRUE(Near(a(r,c), b(r,c), 1e-5f));
}

TEST(Mat4fTest, TranslationThenScale) {
  // T({1,0,0}) * S({2,2,2}): translates then scales.
  // For a column vector [x,y,z,1]:
  // First T: [x+1, y, z, 1], then S: [2(x+1), 2y, 2z, 1]
  // So combined = S * T
  Mat4f m = Mat4f::Scale3D({2.f, 2.f, 2.f}) * Mat4f::Translation({1.f, 0.f, 0.f});
  EXPECT_TRUE(Near(m(0,0), 2.f));
  EXPECT_TRUE(Near(m(0,3), 2.f));   // 2 * 1 = 2
  EXPECT_TRUE(Near(m(3,3), 1.f));
}
