#include "core/Mat2f.h"
#include "core/MathUtils.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace core;

namespace {

bool Near(float a, float b, float eps = 1e-5f) { return NearlyEqual(a, b, eps); }

void ExpectMat2fNear(const Mat2f& m, float m00, float m01, float m10, float m11,
                     float eps = 1e-5f) {
  EXPECT_TRUE(Near(m(0,0), m00, eps));
  EXPECT_TRUE(Near(m(0,1), m01, eps));
  EXPECT_TRUE(Near(m(1,0), m10, eps));
  EXPECT_TRUE(Near(m(1,1), m11, eps));
}

}  // namespace

// ---- Identity ---------------------------------------------------------------

TEST(Mat2fTest, DefaultIsIdentity) {
  Mat2f m;
  ExpectMat2fNear(m, 1.f, 0.f, 0.f, 1.f);
}

TEST(Mat2fTest, kIdentityIsIdentity) {
  ExpectMat2fNear(Mat2f::kIdentity, 1.f, 0.f, 0.f, 1.f);
}

// ---- Element access ---------------------------------------------------------

TEST(Mat2fTest, SetAndGetElement) {
  Mat2f m;
  m(0, 1) = 7.f;
  EXPECT_TRUE(Near(m(0, 1), 7.f));
  EXPECT_TRUE(Near(m(0, 0), 1.f));  // rest unchanged
}

// ---- Multiply ---------------------------------------------------------------

TEST(Mat2fTest, MultiplyByIdentityIsNoOp) {
  Mat2f a{1.f, 2.f, 3.f, 4.f};
  ExpectMat2fNear(a * Mat2f::kIdentity, 1.f, 2.f, 3.f, 4.f);
  ExpectMat2fNear(Mat2f::kIdentity * a, 1.f, 2.f, 3.f, 4.f);
}

TEST(Mat2fTest, KnownMultiplyResult) {
  // [1,2;3,4] * [5,6;7,8] = [19,22;43,50]
  Mat2f a{1.f, 2.f, 3.f, 4.f};
  Mat2f b{5.f, 6.f, 7.f, 8.f};
  ExpectMat2fNear(a * b, 19.f, 22.f, 43.f, 50.f);
}

TEST(Mat2fTest, MultiplyAssignIsConsistent) {
  Mat2f a{1.f, 2.f, 3.f, 4.f};
  Mat2f b{5.f, 6.f, 7.f, 8.f};
  Mat2f expected = a * b;
  a *= b;
  ExpectMat2fNear(a, expected(0,0), expected(0,1), expected(1,0), expected(1,1));
}

// ---- Inverse ----------------------------------------------------------------

TEST(Mat2fTest, InverseOfIdentityIsIdentity) {
  ExpectMat2fNear(Mat2f::kIdentity.Inverse(), 1.f, 0.f, 0.f, 1.f);
}

TEST(Mat2fTest, MultiplyByInverseIsIdentity) {
  Mat2f m{1.f, 2.f, 3.f, 4.f};
  Mat2f result = m * m.Inverse();
  ExpectMat2fNear(result, 1.f, 0.f, 0.f, 1.f, 1e-5f);
}

TEST(Mat2fTest, InverseKnownResult) {
  // det([1,2;3,4]) = -2 → inv = [-2,1; 1.5,-0.5]
  Mat2f m{1.f, 2.f, 3.f, 4.f};
  ExpectMat2fNear(m.Inverse(), -2.f, 1.f, 1.5f, -0.5f);
}

// ---- Factories --------------------------------------------------------------

TEST(Mat2fTest, Rotation2DAtQuarterPi) {
  // cos(π/2)≈0, sin(π/2)=1 → [0,-1; 1,0]
  Mat2f r = Mat2f::Rotation2D(kHalfPi);
  ExpectMat2fNear(r, 0.f, -1.f, 1.f, 0.f, 1e-5f);
}

TEST(Mat2fTest, Rotation2DAtZeroIsIdentity) {
  ExpectMat2fNear(Mat2f::Rotation2D(0.f), 1.f, 0.f, 0.f, 1.f);
}

TEST(Mat2fTest, Scale2D) {
  Mat2f s = Mat2f::Scale2D({2.f, 3.f});
  ExpectMat2fNear(s, 2.f, 0.f, 0.f, 3.f);
}

TEST(Mat2fTest, RotationScale2D) {
  // Scale({2,3}) * Rotation(π/2) = [2,0;0,3] * [0,-1;1,0] = [0,-2;3,0]
  Mat2f rs = Mat2f::RotationScale2D(kHalfPi, {2.f, 3.f});
  ExpectMat2fNear(rs, 0.f, -2.f, 3.f, 0.f, 1e-5f);
}

// ---- Vec2f × Mat2f ----------------------------------------------------------

TEST(Mat2fTest, Vec2fTimesIdentityIsNoOp) {
  Vec2f v{3.f, 4.f};
  Vec2f r = v * Mat2f::kIdentity;
  EXPECT_TRUE(Near(r.x, 3.f));
  EXPECT_TRUE(Near(r.y, 4.f));
}

TEST(Mat2fTest, Vec2fTimesRotation2D_QuarterTurn) {
  // Rotation(π/2) = [0,-1; 1,0].  Applied to (1,0): result[k] = M(k,0)*1.
  Vec2f v{1.f, 0.f};
  Vec2f r = v * Mat2f::Rotation2D(kHalfPi);
  EXPECT_TRUE(Near(r.x, 0.f, 1e-5f));
  EXPECT_TRUE(Near(r.y, 1.f, 1e-5f));
}

TEST(Mat2fTest, Vec2fTimesScale2D) {
  Vec2f r = Vec2f{1.f, 2.f} * Mat2f::Scale2D({3.f, 4.f});
  EXPECT_TRUE(Near(r.x, 3.f));
  EXPECT_TRUE(Near(r.y, 8.f));
}

TEST(Mat2fTest, Vec2fTimesAssignIsConsistent) {
  Vec2f v{2.f, 3.f};
  Vec2f expected = v * Mat2f::Scale2D({5.f, 7.f});
  v *= Mat2f::Scale2D({5.f, 7.f});
  EXPECT_TRUE(Near(v.x, expected.x));
  EXPECT_TRUE(Near(v.y, expected.y));
}
