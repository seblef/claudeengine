#include "core/Vec2f.h"
#include "core/MathUtils.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace core;

namespace {
bool Near(float a, float b, float eps = 1e-5f) { return NearlyEqual(a, b, eps); }
}  // namespace

// ---- SquaredDistance / Distance ---------------------------------------------

TEST(Vec2fTest, SquaredDistanceSamePoint) {
  Vec2f v{1.f, 2.f};
  EXPECT_TRUE(Near(v.SquaredDistance(v), 0.f));
}

TEST(Vec2fTest, SquaredDistanceKnown) {
  Vec2f a{0.f, 0.f}, b{3.f, 4.f};
  EXPECT_TRUE(Near(a.SquaredDistance(b), 25.f));
}

TEST(Vec2fTest, DistanceSamePoint) {
  Vec2f v{5.f, -3.f};
  EXPECT_TRUE(Near(v.Distance(v), 0.f));
}

TEST(Vec2fTest, DistanceKnown) {
  Vec2f a{0.f, 0.f}, b{3.f, 4.f};
  EXPECT_TRUE(Near(a.Distance(b), 5.f));
}

// ---- IsBetween --------------------------------------------------------------

TEST(Vec2fTest, IsBetweenMidpoint) {
  Vec2f a{0.f, 0.f}, b{2.f, 0.f}, mid{1.f, 0.f};
  EXPECT_TRUE(mid.IsBetween(a, b));
}

TEST(Vec2fTest, IsBetweenEndpoint) {
  Vec2f a{0.f, 0.f}, b{2.f, 0.f};
  EXPECT_TRUE(a.IsBetween(a, b));
  EXPECT_TRUE(b.IsBetween(a, b));
}

TEST(Vec2fTest, IsBetweenOffSegment) {
  Vec2f a{0.f, 0.f}, b{2.f, 0.f}, off{1.f, 1.f};
  EXPECT_FALSE(off.IsBetween(a, b));
}

TEST(Vec2fTest, IsBetweenPastEnd) {
  Vec2f a{0.f, 0.f}, b{2.f, 0.f}, past{3.f, 0.f};
  EXPECT_FALSE(past.IsBetween(a, b));
}

// ---- Lerp -------------------------------------------------------------------

TEST(Vec2fTest, LerpAtZeroIsSelf) {
  Vec2f a{1.f, 2.f}, b{5.f, 6.f};
  Vec2f r = a.Lerp(b, 0.f);
  EXPECT_TRUE(Near(r.x, a.x)); EXPECT_TRUE(Near(r.y, a.y));
}

TEST(Vec2fTest, LerpAtOneIsOther) {
  Vec2f a{1.f, 2.f}, b{5.f, 6.f};
  Vec2f r = a.Lerp(b, 1.f);
  EXPECT_TRUE(Near(r.x, b.x)); EXPECT_TRUE(Near(r.y, b.y));
}

TEST(Vec2fTest, LerpAtHalfIsMidpoint) {
  Vec2f a{0.f, 0.f}, b{4.f, 8.f};
  Vec2f r = a.Lerp(b, 0.5f);
  EXPECT_TRUE(Near(r.x, 2.f)); EXPECT_TRUE(Near(r.y, 4.f));
}

// ---- Scale ------------------------------------------------------------------

TEST(Vec2fTest, ScaleKnown) {
  Vec2f v{2.f, 3.f}, s{4.f, 5.f};
  Vec2f r = v.Scale(s);
  EXPECT_TRUE(Near(r.x, 8.f)); EXPECT_TRUE(Near(r.y, 15.f));
}

// ---- InvScale ---------------------------------------------------------------

TEST(Vec2fTest, InvScaleKnown) {
  Vec2f v{6.f, 9.f}, s{2.f, 3.f};
  Vec2f r = v.InvScale(s);
  EXPECT_TRUE(Near(r.x, 3.f)); EXPECT_TRUE(Near(r.y, 3.f));
}

// ---- Inverse / InverseInPlace -----------------------------------------------

TEST(Vec2fTest, InverseKnown) {
  Vec2f v{2.f, 4.f};
  Vec2f r = v.Inverse();
  EXPECT_TRUE(Near(r.x, 0.5f)); EXPECT_TRUE(Near(r.y, 0.25f));
}

TEST(Vec2fTest, InverseInPlace) {
  Vec2f v{2.f, 4.f};
  Vec2f expected = v.Inverse();
  v.InverseInPlace();
  EXPECT_TRUE(Near(v.x, expected.x)); EXPECT_TRUE(Near(v.y, expected.y));
}
