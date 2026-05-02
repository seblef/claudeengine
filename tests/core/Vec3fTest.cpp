#include "core/Vec3f.h"
#include "core/MathUtils.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace core;

namespace {
bool Near(float a, float b, float eps = 1e-5f) { return NearlyEqual(a, b, eps); }
}  // namespace

// ---- SquaredDistance / Distance ---------------------------------------------

TEST(Vec3fTest, SquaredDistanceSamePoint) {
  Vec3f v{1.f, 2.f, 3.f};
  EXPECT_TRUE(Near(v.SquaredDistance(v), 0.f));
}

TEST(Vec3fTest, SquaredDistanceKnown) {
  Vec3f a{0.f, 0.f, 0.f}, b{1.f, 2.f, 2.f};
  EXPECT_TRUE(Near(a.SquaredDistance(b), 9.f));
}

TEST(Vec3fTest, DistanceSamePoint) {
  Vec3f v{-1.f, 5.f, 3.f};
  EXPECT_TRUE(Near(v.Distance(v), 0.f));
}

TEST(Vec3fTest, DistanceKnown) {
  Vec3f a{0.f, 0.f, 0.f}, b{1.f, 2.f, 2.f};
  EXPECT_TRUE(Near(a.Distance(b), 3.f));
}

// ---- IsBetween --------------------------------------------------------------

TEST(Vec3fTest, IsBetweenMidpoint) {
  Vec3f a{0.f, 0.f, 0.f}, b{0.f, 0.f, 4.f}, mid{0.f, 0.f, 2.f};
  EXPECT_TRUE(mid.IsBetween(a, b));
}

TEST(Vec3fTest, IsBetweenEndpoints) {
  Vec3f a{1.f, 0.f, 0.f}, b{3.f, 0.f, 0.f};
  EXPECT_TRUE(a.IsBetween(a, b));
  EXPECT_TRUE(b.IsBetween(a, b));
}

TEST(Vec3fTest, IsBetweenOffSegment) {
  Vec3f a{0.f, 0.f, 0.f}, b{4.f, 0.f, 0.f}, off{2.f, 1.f, 0.f};
  EXPECT_FALSE(off.IsBetween(a, b));
}

TEST(Vec3fTest, IsBetweenPastEnd) {
  Vec3f a{0.f, 0.f, 0.f}, b{1.f, 0.f, 0.f}, past{2.f, 0.f, 0.f};
  EXPECT_FALSE(past.IsBetween(a, b));
}

// ---- Lerp -------------------------------------------------------------------

TEST(Vec3fTest, LerpAtZeroIsSelf) {
  Vec3f a{1.f, 2.f, 3.f}, b{5.f, 6.f, 7.f};
  Vec3f r = a.Lerp(b, 0.f);
  EXPECT_TRUE(Near(r.x, a.x)); EXPECT_TRUE(Near(r.y, a.y)); EXPECT_TRUE(Near(r.z, a.z));
}

TEST(Vec3fTest, LerpAtOneIsOther) {
  Vec3f a{1.f, 2.f, 3.f}, b{5.f, 6.f, 7.f};
  Vec3f r = a.Lerp(b, 1.f);
  EXPECT_TRUE(Near(r.x, b.x)); EXPECT_TRUE(Near(r.y, b.y)); EXPECT_TRUE(Near(r.z, b.z));
}

TEST(Vec3fTest, LerpAtHalfIsMidpoint) {
  Vec3f a{0.f, 0.f, 0.f}, b{4.f, 8.f, 12.f};
  Vec3f r = a.Lerp(b, 0.5f);
  EXPECT_TRUE(Near(r.x, 2.f)); EXPECT_TRUE(Near(r.y, 4.f)); EXPECT_TRUE(Near(r.z, 6.f));
}

// ---- Scale ------------------------------------------------------------------

TEST(Vec3fTest, ScaleKnown) {
  Vec3f r = Vec3f{2.f, 3.f, 4.f}.Scale({5.f, 6.f, 7.f});
  EXPECT_TRUE(Near(r.x, 10.f)); EXPECT_TRUE(Near(r.y, 18.f)); EXPECT_TRUE(Near(r.z, 28.f));
}

TEST(Vec3fTest, ScalePreservesW) {
  // Verify the w_ invariant: Scale must not corrupt the hidden w_ padding.
  Vec3f v{1.f, 2.f, 3.f};
  Vec3f r = v.Scale({2.f, 2.f, 2.f});
  // Reconstruct through Reg to ensure the 4th float stayed 0.
  EXPECT_TRUE(Near(r.Length(), Vec3f{2.f, 4.f, 6.f}.Length()));
}

// ---- InvScale ---------------------------------------------------------------

TEST(Vec3fTest, InvScaleKnown) {
  Vec3f r = Vec3f{6.f, 9.f, 12.f}.InvScale({2.f, 3.f, 4.f});
  EXPECT_TRUE(Near(r.x, 3.f)); EXPECT_TRUE(Near(r.y, 3.f)); EXPECT_TRUE(Near(r.z, 3.f));
}

// ---- Inverse / InverseInPlace -----------------------------------------------

TEST(Vec3fTest, InverseKnown) {
  Vec3f r = Vec3f{2.f, 4.f, 8.f}.Inverse();
  EXPECT_TRUE(Near(r.x, 0.5f)); EXPECT_TRUE(Near(r.y, 0.25f)); EXPECT_TRUE(Near(r.z, 0.125f));
}

TEST(Vec3fTest, InverseInPlace) {
  Vec3f v{2.f, 4.f, 8.f};
  Vec3f expected = v.Inverse();
  v.InverseInPlace();
  EXPECT_TRUE(Near(v.x, expected.x)); EXPECT_TRUE(Near(v.y, expected.y));
  EXPECT_TRUE(Near(v.z, expected.z));
}
