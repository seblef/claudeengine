#include "core/Vec4f.h"
#include "core/MathUtils.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace core;

namespace {
bool Near(float a, float b, float eps = 1e-5f) { return NearlyEqual(a, b, eps); }
}  // namespace

// ---- SquaredDistance / Distance ---------------------------------------------

TEST(Vec4fTest, SquaredDistanceSamePoint) {
  Vec4f v{1.f, 2.f, 3.f, 4.f};
  EXPECT_TRUE(Near(v.SquaredDistance(v), 0.f));
}

TEST(Vec4fTest, SquaredDistanceKnown) {
  Vec4f a{0.f, 0.f, 0.f, 0.f}, b{1.f, 0.f, 0.f, 0.f};
  EXPECT_TRUE(Near(a.SquaredDistance(b), 1.f));
}

TEST(Vec4fTest, DistanceSamePoint) {
  Vec4f v{3.f, -1.f, 0.f, 2.f};
  EXPECT_TRUE(Near(v.Distance(v), 0.f));
}

TEST(Vec4fTest, DistanceKnown) {
  Vec4f a{0.f, 0.f, 0.f, 0.f}, b{2.f, 0.f, 0.f, 0.f};
  EXPECT_TRUE(Near(a.Distance(b), 2.f));
}

// ---- IsBetween --------------------------------------------------------------

TEST(Vec4fTest, IsBetweenMidpoint) {
  Vec4f a{0.f, 0.f, 0.f, 0.f}, b{2.f, 2.f, 2.f, 2.f}, mid{1.f, 1.f, 1.f, 1.f};
  EXPECT_TRUE(mid.IsBetween(a, b));
}

TEST(Vec4fTest, IsBetweenEndpoints) {
  Vec4f a{0.f, 0.f, 0.f, 0.f}, b{1.f, 1.f, 1.f, 1.f};
  EXPECT_TRUE(a.IsBetween(a, b));
  EXPECT_TRUE(b.IsBetween(a, b));
}

TEST(Vec4fTest, IsBetweenOffSegment) {
  Vec4f a{0.f, 0.f, 0.f, 0.f}, b{2.f, 0.f, 0.f, 0.f}, off{1.f, 1.f, 0.f, 0.f};
  EXPECT_FALSE(off.IsBetween(a, b));
}

TEST(Vec4fTest, IsBetweenPastEnd) {
  Vec4f a{0.f, 0.f, 0.f, 0.f}, b{1.f, 0.f, 0.f, 0.f}, past{2.f, 0.f, 0.f, 0.f};
  EXPECT_FALSE(past.IsBetween(a, b));
}

// ---- Lerp -------------------------------------------------------------------

TEST(Vec4fTest, LerpAtZeroIsSelf) {
  Vec4f a{1.f, 2.f, 3.f, 4.f}, b{5.f, 6.f, 7.f, 8.f};
  Vec4f r = a.Lerp(b, 0.f);
  EXPECT_TRUE(Near(r.x, a.x)); EXPECT_TRUE(Near(r.y, a.y));
  EXPECT_TRUE(Near(r.z, a.z)); EXPECT_TRUE(Near(r.w, a.w));
}

TEST(Vec4fTest, LerpAtOneIsOther) {
  Vec4f a{1.f, 2.f, 3.f, 4.f}, b{5.f, 6.f, 7.f, 8.f};
  Vec4f r = a.Lerp(b, 1.f);
  EXPECT_TRUE(Near(r.x, b.x)); EXPECT_TRUE(Near(r.y, b.y));
  EXPECT_TRUE(Near(r.z, b.z)); EXPECT_TRUE(Near(r.w, b.w));
}

TEST(Vec4fTest, LerpAtHalfIsMidpoint) {
  Vec4f a{0.f, 0.f, 0.f, 0.f}, b{4.f, 8.f, 12.f, 16.f};
  Vec4f r = a.Lerp(b, 0.5f);
  EXPECT_TRUE(Near(r.x, 2.f)); EXPECT_TRUE(Near(r.y, 4.f));
  EXPECT_TRUE(Near(r.z, 6.f)); EXPECT_TRUE(Near(r.w, 8.f));
}

// ---- Scale ------------------------------------------------------------------

TEST(Vec4fTest, ScaleKnown) {
  Vec4f r = Vec4f{1.f, 2.f, 3.f, 4.f}.Scale({2.f, 3.f, 4.f, 5.f});
  EXPECT_TRUE(Near(r.x, 2.f)); EXPECT_TRUE(Near(r.y, 6.f));
  EXPECT_TRUE(Near(r.z, 12.f)); EXPECT_TRUE(Near(r.w, 20.f));
}

// ---- InvScale ---------------------------------------------------------------

TEST(Vec4fTest, InvScaleKnown) {
  Vec4f r = Vec4f{6.f, 9.f, 12.f, 20.f}.InvScale({2.f, 3.f, 4.f, 5.f});
  EXPECT_TRUE(Near(r.x, 3.f)); EXPECT_TRUE(Near(r.y, 3.f));
  EXPECT_TRUE(Near(r.z, 3.f)); EXPECT_TRUE(Near(r.w, 4.f));
}

// ---- Inverse / InverseInPlace -----------------------------------------------

TEST(Vec4fTest, InverseKnown) {
  Vec4f r = Vec4f{2.f, 4.f, 8.f, 16.f}.Inverse();
  EXPECT_TRUE(Near(r.x, 0.5f));   EXPECT_TRUE(Near(r.y, 0.25f));
  EXPECT_TRUE(Near(r.z, 0.125f)); EXPECT_TRUE(Near(r.w, 0.0625f));
}

TEST(Vec4fTest, InverseInPlace) {
  Vec4f v{2.f, 4.f, 8.f, 16.f};
  Vec4f expected = v.Inverse();
  v.InverseInPlace();
  EXPECT_TRUE(Near(v.x, expected.x)); EXPECT_TRUE(Near(v.y, expected.y));
  EXPECT_TRUE(Near(v.z, expected.z)); EXPECT_TRUE(Near(v.w, expected.w));
}
