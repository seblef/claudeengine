#include "core/Mat3f.h"
#include "core/MathUtils.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace core;

namespace {

bool Near(float a, float b, float eps = 1e-5f) { return NearlyEqual(a, b, eps); }

void ExpectIdentity3(const Mat3f& m, float eps = 1e-5f) {
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(m(r,c), r == c ? 1.f : 0.f, eps));
}

void ExpectZero3(const Mat3f& m, float eps = 1e-5f) {
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(m(r,c), 0.f, eps));
}

}  // namespace

// ---- Identity ---------------------------------------------------------------

TEST(Mat3fTest, DefaultIsIdentity) {
  ExpectIdentity3(Mat3f{});
}

TEST(Mat3fTest, kIdentityIsIdentity) {
  ExpectIdentity3(Mat3f::kIdentity);
}

// ---- Element access ---------------------------------------------------------

TEST(Mat3fTest, SetAndGetElement) {
  Mat3f m;
  m(1, 2) = 9.f;
  EXPECT_TRUE(Near(m(1, 2), 9.f));
  EXPECT_TRUE(Near(m(1, 1), 1.f));  // rest unchanged
}

// ---- Multiply ---------------------------------------------------------------

TEST(Mat3fTest, MultiplyByIdentityIsNoOp) {
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) {
      EXPECT_TRUE(Near((a * Mat3f::kIdentity)(r,c), a(r,c)));
      EXPECT_TRUE(Near((Mat3f::kIdentity * a)(r,c), a(r,c)));
    }
}

TEST(Mat3fTest, KnownMultiplyResult) {
  // A=[1..9 row-major], B=[9..1 row-major]
  // C = A*B: see hand-computed values below.
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat3f b{9.f,8.f,7.f, 6.f,5.f,4.f, 3.f,2.f,1.f};
  Mat3f c = a * b;
  EXPECT_TRUE(Near(c(0,0), 30.f));
  EXPECT_TRUE(Near(c(0,1), 24.f));
  EXPECT_TRUE(Near(c(0,2), 18.f));
  EXPECT_TRUE(Near(c(1,0), 84.f));
  EXPECT_TRUE(Near(c(1,1), 69.f));
  EXPECT_TRUE(Near(c(1,2), 54.f));
  EXPECT_TRUE(Near(c(2,0), 138.f));
  EXPECT_TRUE(Near(c(2,1), 114.f));
  EXPECT_TRUE(Near(c(2,2), 90.f));
}

TEST(Mat3fTest, MultiplyAssignIsConsistent) {
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat3f b{9.f,8.f,7.f, 6.f,5.f,4.f, 3.f,2.f,1.f};
  Mat3f expected = a * b;
  a *= b;
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(a(r,c), expected(r,c)));
}

// ---- Add / subtract ---------------------------------------------------------

TEST(Mat3fTest, AddIsDoubleOfSelf) {
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat3f result = a + a;
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(result(r,c), 2.f * a(r,c)));
}

TEST(Mat3fTest, SubtractSelfIsZero) {
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  ExpectZero3(a - a);
}

TEST(Mat3fTest, KnownAddResult) {
  // [1..9] + [9..1] = [10,10,10, 10,10,10, 10,10,10]
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat3f b{9.f,8.f,7.f, 6.f,5.f,4.f, 3.f,2.f,1.f};
  Mat3f c = a + b;
  for (int r = 0; r < 3; ++r)
    for (int c2 = 0; c2 < 3; ++c2)
      EXPECT_TRUE(Near(c(r,c2), 10.f));
}

TEST(Mat3fTest, AddAssignIsConsistent) {
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat3f b{9.f,8.f,7.f, 6.f,5.f,4.f, 3.f,2.f,1.f};
  Mat3f expected = a + b;
  a += b;
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(a(r,c), expected(r,c)));
}

TEST(Mat3fTest, SubtractAssignIsConsistent) {
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat3f b{9.f,8.f,7.f, 6.f,5.f,4.f, 3.f,2.f,1.f};
  Mat3f expected = a - b;
  a -= b;
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(a(r,c), expected(r,c)));
}

// ---- Inverse ----------------------------------------------------------------

TEST(Mat3fTest, InverseOfIdentityIsIdentity) {
  ExpectIdentity3(Mat3f::kIdentity.Inverse());
}

TEST(Mat3fTest, MultiplyByInverseIsIdentity) {
  // M = [1,0,1; 0,2,0; 1,0,3] — det = 4
  Mat3f m{1.f,0.f,1.f, 0.f,2.f,0.f, 1.f,0.f,3.f};
  ExpectIdentity3(m * m.Inverse(), 1e-5f);
}

TEST(Mat3fTest, InverseKnownResult) {
  // M = [1,0,1; 0,2,0; 1,0,3] → inv = [1.5,0,-0.5; 0,0.5,0; -0.5,0,0.5]
  Mat3f m{1.f,0.f,1.f, 0.f,2.f,0.f, 1.f,0.f,3.f};
  Mat3f inv = m.Inverse();
  EXPECT_TRUE(Near(inv(0,0),  1.5f));
  EXPECT_TRUE(Near(inv(0,1),  0.f));
  EXPECT_TRUE(Near(inv(0,2), -0.5f));
  EXPECT_TRUE(Near(inv(1,0),  0.f));
  EXPECT_TRUE(Near(inv(1,1),  0.5f));
  EXPECT_TRUE(Near(inv(1,2),  0.f));
  EXPECT_TRUE(Near(inv(2,0), -0.5f));
  EXPECT_TRUE(Near(inv(2,1),  0.f));
  EXPECT_TRUE(Near(inv(2,2),  0.5f));
}

// ---- Transpose --------------------------------------------------------------

TEST(Mat3fTest, TransposeSwapsOffDiagonal) {
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat3f t = a.Transpose();
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(t(r,c), a(c,r)));
}

TEST(Mat3fTest, TransposeOfTransposeIsOriginal) {
  Mat3f a{1.f,2.f,3.f, 4.f,5.f,6.f, 7.f,8.f,9.f};
  Mat3f result = a.Transpose().Transpose();
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(result(r,c), a(r,c)));
}

// ---- Factories --------------------------------------------------------------

TEST(Mat3fTest, Scale3D) {
  Mat3f s = Mat3f::Scale3D({2.f, 3.f, 4.f});
  EXPECT_TRUE(Near(s(0,0), 2.f)); EXPECT_TRUE(Near(s(0,1), 0.f)); EXPECT_TRUE(Near(s(0,2), 0.f));
  EXPECT_TRUE(Near(s(1,0), 0.f)); EXPECT_TRUE(Near(s(1,1), 3.f)); EXPECT_TRUE(Near(s(1,2), 0.f));
  EXPECT_TRUE(Near(s(2,0), 0.f)); EXPECT_TRUE(Near(s(2,1), 0.f)); EXPECT_TRUE(Near(s(2,2), 4.f));
}

TEST(Mat3fTest, RotationXHalfPi) {
  // Rx(π/2) = [1,0,0; 0,0,-1; 0,1,0]
  Mat3f r = Mat3f::RotationX(kHalfPi);
  EXPECT_TRUE(Near(r(0,0), 1.f, 1e-5f)); EXPECT_TRUE(Near(r(0,1), 0.f)); EXPECT_TRUE(Near(r(0,2), 0.f));
  EXPECT_TRUE(Near(r(1,0), 0.f)); EXPECT_TRUE(Near(r(1,1), 0.f, 1e-5f)); EXPECT_TRUE(Near(r(1,2), -1.f, 1e-5f));
  EXPECT_TRUE(Near(r(2,0), 0.f)); EXPECT_TRUE(Near(r(2,1), 1.f, 1e-5f)); EXPECT_TRUE(Near(r(2,2), 0.f, 1e-5f));
}

TEST(Mat3fTest, RotationYHalfPi) {
  // Ry(π/2) = [0,0,1; 0,1,0; -1,0,0]
  Mat3f r = Mat3f::RotationY(kHalfPi);
  EXPECT_TRUE(Near(r(0,0), 0.f, 1e-5f)); EXPECT_TRUE(Near(r(0,1), 0.f)); EXPECT_TRUE(Near(r(0,2), 1.f, 1e-5f));
  EXPECT_TRUE(Near(r(1,0), 0.f)); EXPECT_TRUE(Near(r(1,1), 1.f)); EXPECT_TRUE(Near(r(1,2), 0.f));
  EXPECT_TRUE(Near(r(2,0), -1.f, 1e-5f)); EXPECT_TRUE(Near(r(2,1), 0.f)); EXPECT_TRUE(Near(r(2,2), 0.f, 1e-5f));
}

TEST(Mat3fTest, RotationZHalfPi) {
  // Rz(π/2) = [0,-1,0; 1,0,0; 0,0,1]
  Mat3f r = Mat3f::RotationZ(kHalfPi);
  EXPECT_TRUE(Near(r(0,0), 0.f, 1e-5f)); EXPECT_TRUE(Near(r(0,1), -1.f, 1e-5f)); EXPECT_TRUE(Near(r(0,2), 0.f));
  EXPECT_TRUE(Near(r(1,0), 1.f, 1e-5f)); EXPECT_TRUE(Near(r(1,1), 0.f, 1e-5f)); EXPECT_TRUE(Near(r(1,2), 0.f));
  EXPECT_TRUE(Near(r(2,0), 0.f)); EXPECT_TRUE(Near(r(2,1), 0.f)); EXPECT_TRUE(Near(r(2,2), 1.f));
}

TEST(Mat3fTest, RotationZeroIsIdentity) {
  ExpectIdentity3(Mat3f::Rotation({0.f, 0.f, 0.f}));
}

TEST(Mat3fTest, RotationXAxisMatchesRotationX) {
  // Rotation({π/3, 0, 0}) == RotationX(π/3)
  float angle = kPi / 3.f;
  Mat3f a = Mat3f::Rotation({angle, 0.f, 0.f});
  Mat3f b = Mat3f::RotationX(angle);
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      EXPECT_TRUE(Near(a(r,c), b(r,c), 1e-5f));
}
