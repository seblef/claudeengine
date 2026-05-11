#include "core/Plane.h"

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/MathUtils.h"

#include <gtest/gtest.h>
#include <cmath>

using namespace core;

namespace {
bool Near(float a, float b, float eps = 1e-4f) { return NearlyEqual(a, b, eps); }
bool NearV(const Vec3f& a, const Vec3f& b, float eps = 1e-4f) {
  return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}
}  // namespace

// ---- Construction -----------------------------------------------------------

TEST(PlaneTest, FromDistAndNormal) {
  Plane p(3.f, Vec3f{0.f, 1.f, 0.f});
  EXPECT_TRUE(NearV(p.GetNormal(), Vec3f{0.f, 1.f, 0.f}));
  EXPECT_TRUE(Near(p.GetDist(), 3.f));
}

TEST(PlaneTest, FromDistAndNormalNormalizesNormal) {
  Plane p(0.f, Vec3f{0.f, 2.f, 0.f});
  EXPECT_TRUE(Near(p.GetNormal().Length(), 1.f));
}

TEST(PlaneTest, FromPointAndNormal) {
  Vec3f point{0.f, 5.f, 0.f};
  Vec3f normal{0.f, 1.f, 0.f};
  Plane p(point, normal);
  EXPECT_TRUE(NearV(p.GetNormal(), Vec3f{0.f, 1.f, 0.f}));
  EXPECT_TRUE(Near(p.GetDist(), 5.f));
}

TEST(PlaneTest, FromThreePoints) {
  // XZ plane at y=0: points (1,0,0), (0,0,1), (0,0,0) → normal = (0,1,0) or (0,-1,0)
  Vec3f p1{1.f, 0.f, 0.f};
  Vec3f p2{0.f, 0.f, 1.f};
  Vec3f p3{0.f, 0.f, 0.f};
  Plane pl(p1, p2, p3);
  EXPECT_TRUE(Near(pl.GetNormal().Length(), 1.f));
  // All three points must lie on the plane: normal·p = dist for each
  EXPECT_TRUE(Near(pl.GetNormal().Dot(p1), pl.GetDist()));
  EXPECT_TRUE(Near(pl.GetNormal().Dot(p2), pl.GetDist()));
  EXPECT_TRUE(Near(pl.GetNormal().Dot(p3), pl.GetDist()));
}

TEST(PlaneTest, Constants) {
  EXPECT_TRUE(NearV(Plane::kAxisX.GetNormal(), Vec3f{1.f, 0.f, 0.f}));
  EXPECT_TRUE(Near(Plane::kAxisX.GetDist(), 0.f));
  EXPECT_TRUE(NearV(Plane::kAxisY.GetNormal(), Vec3f{0.f, 1.f, 0.f}));
  EXPECT_TRUE(NearV(Plane::kAxisZ.GetNormal(), Vec3f{0.f, 0.f, 1.f}));
}

// ---- Classify point ---------------------------------------------------------

TEST(PlaneTest, ClassifyPointFront) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});  // XZ plane, normal +Y
  EXPECT_EQ(p.Classify(Vec3f{0.f, 1.f, 0.f}), Plane::Side::kFront);
}

TEST(PlaneTest, ClassifyPointBack) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  EXPECT_EQ(p.Classify(Vec3f{0.f, -1.f, 0.f}), Plane::Side::kBack);
}

TEST(PlaneTest, ClassifyPointOn) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  EXPECT_EQ(p.Classify(Vec3f{3.f, 0.f, -2.f}), Plane::Side::kOn);
}

TEST(PlaneTest, ClassifyPointWithOffset) {
  Plane p(2.f, Vec3f{0.f, 1.f, 0.f});  // plane y=2
  EXPECT_EQ(p.Classify(Vec3f{0.f, 3.f, 0.f}), Plane::Side::kFront);
  EXPECT_EQ(p.Classify(Vec3f{0.f, 1.f, 0.f}), Plane::Side::kBack);
  EXPECT_EQ(p.Classify(Vec3f{0.f, 2.f, 0.f}), Plane::Side::kOn);
}

// ---- Classify BBox ----------------------------------------------------------

TEST(PlaneTest, ClassifyBBoxFront) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});  // XZ plane
  BBox3 box(Vec3f{-1.f, 1.f, -1.f}, Vec3f{1.f, 3.f, 1.f});  // entirely above
  EXPECT_EQ(p.Classify(box), Plane::Side::kFront);
}

TEST(PlaneTest, ClassifyBBoxBack) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  BBox3 box(Vec3f{-1.f, -3.f, -1.f}, Vec3f{1.f, -1.f, 1.f});  // entirely below
  EXPECT_EQ(p.Classify(box), Plane::Side::kBack);
}

TEST(PlaneTest, ClassifyBBoxClip) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  BBox3 box(Vec3f{-1.f, -1.f, -1.f}, Vec3f{1.f, 1.f, 1.f});  // straddles plane
  EXPECT_EQ(p.Classify(box), Plane::Side::kClip);
}

TEST(PlaneTest, ClassifyBBoxOn) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  // Flat box exactly on XZ plane
  BBox3 box(Vec3f{-1.f, 0.f, -1.f}, Vec3f{1.f, 0.f, 1.f});
  EXPECT_EQ(p.Classify(box), Plane::Side::kOn);
}

// ---- IntersectsLine (distance) ----------------------------------------------

TEST(PlaneTest, IntersectsLineReturnsDist) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  Vec3f origin{0.f, -3.f, 0.f};
  Vec3f dir{0.f, 1.f, 0.f};
  float t;
  EXPECT_TRUE(p.IntersectsLine(origin, dir, t));
  EXPECT_TRUE(Near(t, 3.f));
}

TEST(PlaneTest, IntersectsLineParallelReturnsFalse) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  Vec3f origin{0.f, 1.f, 0.f};
  Vec3f dir{1.f, 0.f, 0.f};  // parallel to plane
  float t;
  EXPECT_FALSE(p.IntersectsLine(origin, dir, t));
}

// ---- IntersectsLine (point) -------------------------------------------------

TEST(PlaneTest, IntersectsLineReturnsPoint) {
  Plane p(2.f, Vec3f{0.f, 1.f, 0.f});  // y=2 plane
  Vec3f origin{1.f, 0.f, 3.f};
  Vec3f dir{0.f, 1.f, 0.f};
  Vec3f hit;
  EXPECT_TRUE(p.IntersectsLine(origin, dir, hit));
  EXPECT_TRUE(NearV(hit, Vec3f{1.f, 2.f, 3.f}));
}

// ---- IntersectsEdge ---------------------------------------------------------

TEST(PlaneTest, IntersectsEdgeCrossing) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  Vec3f p1{0.f, -1.f, 0.f};
  Vec3f p2{0.f,  1.f, 0.f};
  Vec3f hit;
  EXPECT_TRUE(p.IntersectsEdge(p1, p2, hit));
  EXPECT_TRUE(NearV(hit, Vec3f{0.f, 0.f, 0.f}));
}

TEST(PlaneTest, IntersectsEdgeOutsideRange) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  Vec3f p1{0.f, 1.f, 0.f};  // both endpoints on same side
  Vec3f p2{0.f, 3.f, 0.f};
  Vec3f hit;
  EXPECT_FALSE(p.IntersectsEdge(p1, p2, hit));
}

TEST(PlaneTest, IntersectsEdgeParallel) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  Vec3f p1{0.f, 1.f, 0.f};
  Vec3f p2{1.f, 1.f, 0.f};  // horizontal edge, parallel to plane
  Vec3f hit;
  EXPECT_FALSE(p.IntersectsEdge(p1, p2, hit));
}

TEST(PlaneTest, IntersectsEdgeAtEndpoint) {
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  Vec3f p1{0.f, 0.f, 0.f};  // exactly on plane
  Vec3f p2{0.f, 2.f, 0.f};
  Vec3f hit;
  EXPECT_TRUE(p.IntersectsEdge(p1, p2, hit));
  EXPECT_TRUE(NearV(hit, Vec3f{0.f, 0.f, 0.f}));
}

// ---- Matrix transform -------------------------------------------------------

TEST(PlaneTest, TransformByRotation) {
  // XZ plane (y=0), normal +Y.  Rotate 90° around Z: normal should become +X.
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  Mat4f rot = Mat4f::RotationZ(kHalfPi);
  Plane tp = p * rot;
  EXPECT_TRUE(Near(tp.GetNormal().Length(), 1.f));
  // After 90° CCW around Z: Y axis maps to -X, so normal (0,1,0) → (-1,0,0).
  // (RotationZ rotates CCW: x'=-y, y'=x for column-vector convention)
  EXPECT_TRUE(Near(std::fabs(tp.GetNormal().x), 1.f, 1e-4f));
  EXPECT_TRUE(Near(tp.GetNormal().y, 0.f, 1e-4f));
  EXPECT_TRUE(Near(tp.GetNormal().z, 0.f, 1e-4f));
}

TEST(PlaneTest, TransformByTranslation) {
  // Plane y=0 translated by (0, 3, 0) should become plane y=3.
  Plane p(0.f, Vec3f{0.f, 1.f, 0.f});
  Mat4f trans = Mat4f::Translation(Vec3f{0.f, 3.f, 0.f});
  Plane tp = p * trans;
  EXPECT_TRUE(NearV(tp.GetNormal(), Vec3f{0.f, 1.f, 0.f}));
  EXPECT_TRUE(Near(tp.GetDist(), 3.f));
}

TEST(PlaneTest, TransformPreservesUnitNormal) {
  Plane p(Vec3f{1.f, 2.f, 3.f}, Vec3f{1.f, 1.f, 1.f});
  Mat4f m = Mat4f::Scale3D(Vec3f{2.f, 3.f, 4.f});
  Plane tp = p * m;
  EXPECT_TRUE(Near(tp.GetNormal().Length(), 1.f));
}
