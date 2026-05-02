#include "core/Color.h"
#include "core/MathUtils.h"
#include <gtest/gtest.h>

using namespace core;

namespace {
bool Near(float a, float b, float eps = 1e-5f) { return NearlyEqual(a, b, eps); }
void ExpectColorNear(const Color& c, float r, float g, float b, float a,
                     float eps = 1e-5f) {
  EXPECT_TRUE(Near(c.r, r, eps));
  EXPECT_TRUE(Near(c.g, g, eps));
  EXPECT_TRUE(Near(c.b, b, eps));
  EXPECT_TRUE(Near(c.a, a, eps));
}
}  // namespace

// ---- Construction / constants -----------------------------------------------

TEST(ColorTest, DefaultIsOpaqueBlack) {
  ExpectColorNear(Color{}, 0.f, 0.f, 0.f, 1.f);
}

TEST(ColorTest, kBlack) {
  ExpectColorNear(Color::kBlack, 0.f, 0.f, 0.f, 1.f);
}

TEST(ColorTest, kWhite) {
  ExpectColorNear(Color::kWhite, 1.f, 1.f, 1.f, 1.f);
}

TEST(ColorTest, kTransparent) {
  ExpectColorNear(Color::kTransparent, 0.f, 0.f, 0.f, 0.f);
}

TEST(ColorTest, ConstructRGBADefaultAlpha) {
  Color c{0.2f, 0.4f, 0.6f};
  ExpectColorNear(c, 0.2f, 0.4f, 0.6f, 1.f);
}

TEST(ColorTest, BroadcastScalar) {
  ExpectColorNear(Color{0.5f}, 0.5f, 0.5f, 0.5f, 0.5f);
}

// ---- Addition / subtraction -------------------------------------------------

TEST(ColorTest, AddKnown) {
  Color a{0.1f, 0.2f, 0.3f, 0.4f}, b{0.5f, 0.4f, 0.3f, 0.2f};
  ExpectColorNear(a + b, 0.6f, 0.6f, 0.6f, 0.6f);
}

TEST(ColorTest, SubtractKnown) {
  Color a{1.f, 1.f, 1.f, 1.f}, b{0.2f, 0.3f, 0.4f, 0.5f};
  ExpectColorNear(a - b, 0.8f, 0.7f, 0.6f, 0.5f);
}

TEST(ColorTest, UnaryNegate) {
  Color c{0.25f, 0.5f, 0.75f, 1.f};
  ExpectColorNear(-c, -0.25f, -0.5f, -0.75f, -1.f);
}

// ---- Multiplication / division by Color ------------------------------------

TEST(ColorTest, MultiplyColorHadamard) {
  Color a{0.5f, 1.f, 0.f, 1.f}, b{0.8f, 0.5f, 1.f, 0.5f};
  ExpectColorNear(a * b, 0.4f, 0.5f, 0.f, 0.5f);
}

TEST(ColorTest, DivideColorComponentWise) {
  Color a{1.f, 0.5f, 0.75f, 1.f}, b{2.f, 0.5f, 3.f, 1.f};
  ExpectColorNear(a / b, 0.5f, 1.f, 0.25f, 1.f);
}

// ---- Multiplication / division by scalar ------------------------------------

TEST(ColorTest, MultiplyByScalar) {
  Color c{0.5f, 0.25f, 1.f, 0.8f};
  ExpectColorNear(c * 2.f, 1.f, 0.5f, 2.f, 1.6f);
}

TEST(ColorTest, ScalarTimesColor) {
  Color c{0.5f, 0.25f, 1.f, 0.8f};
  ExpectColorNear(2.f * c, 1.f, 0.5f, 2.f, 1.6f);
}

TEST(ColorTest, DivideByScalar) {
  Color c{1.f, 0.5f, 0.25f, 1.f};
  ExpectColorNear(c / 2.f, 0.5f, 0.25f, 0.125f, 0.5f);
}

// ---- In-place operators -----------------------------------------------------

TEST(ColorTest, AddAssignIsConsistent) {
  Color a{0.1f, 0.2f, 0.3f, 0.4f}, b{0.5f, 0.4f, 0.3f, 0.2f};
  Color expected = a + b;
  a += b;
  ExpectColorNear(a, expected.r, expected.g, expected.b, expected.a);
}

TEST(ColorTest, SubtractAssignIsConsistent) {
  Color a{1.f, 1.f, 1.f, 1.f}, b{0.2f, 0.3f, 0.4f, 0.5f};
  Color expected = a - b;
  a -= b;
  ExpectColorNear(a, expected.r, expected.g, expected.b, expected.a);
}

TEST(ColorTest, MultiplyAssignColorIsConsistent) {
  Color a{0.5f, 1.f, 0.f, 1.f}, b{0.8f, 0.5f, 1.f, 0.5f};
  Color expected = a * b;
  a *= b;
  ExpectColorNear(a, expected.r, expected.g, expected.b, expected.a);
}

TEST(ColorTest, MultiplyAssignScalarIsConsistent) {
  Color c{0.5f, 0.25f, 1.f, 0.8f};
  Color expected = c * 2.f;
  c *= 2.f;
  ExpectColorNear(c, expected.r, expected.g, expected.b, expected.a);
}

TEST(ColorTest, DivideAssignScalarIsConsistent) {
  Color c{1.f, 0.5f, 0.25f, 1.f};
  Color expected = c / 2.f;
  c /= 2.f;
  ExpectColorNear(c, expected.r, expected.g, expected.b, expected.a);
}

// ---- Comparison -------------------------------------------------------------

TEST(ColorTest, EqualityTrue) {
  Color a{0.5f, 0.3f, 0.1f, 1.f};
  EXPECT_TRUE(a == a);
  Color same{0.5f, 0.3f, 0.1f, 1.f};
  EXPECT_TRUE(a == same);
}

TEST(ColorTest, EqualityFalse) {
  EXPECT_FALSE(Color::kBlack == Color::kWhite);
}

TEST(ColorTest, InequalityTrue) {
  EXPECT_TRUE(Color::kBlack != Color::kWhite);
}

TEST(ColorTest, InequalityFalse) {
  Color a{0.5f, 0.3f, 0.1f, 1.f};
  EXPECT_FALSE(a != a);
}

// ---- Lerp -------------------------------------------------------------------

TEST(ColorTest, LerpAtZeroIsSelf) {
  Color a = Color::kBlack, b = Color::kWhite;
  ExpectColorNear(a.Lerp(b, 0.f), 0.f, 0.f, 0.f, 1.f);
}

TEST(ColorTest, LerpAtOneIsOther) {
  Color a = Color::kBlack, b = Color::kWhite;
  ExpectColorNear(a.Lerp(b, 1.f), 1.f, 1.f, 1.f, 1.f);
}

TEST(ColorTest, LerpAtHalfIsMidpoint) {
  Color a{0.f, 0.f, 0.f, 0.f}, b{1.f, 1.f, 1.f, 1.f};
  ExpectColorNear(a.Lerp(b, 0.5f), 0.5f, 0.5f, 0.5f, 0.5f);
}
