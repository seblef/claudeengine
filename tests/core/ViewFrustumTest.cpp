#include "core/ViewFrustum.h"

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/MathUtils.h"
#include "core/Plane.h"

#include <gtest/gtest.h>

using namespace core;

namespace {

// Standard test frustum: camera at (0,0,10) looking at origin.
// RH perspective, 90-degree FOV, 1:1 aspect, z_near=1, z_far=100.
//
// In view space the camera looks down -Z.  A world point p has:
//   view_z = p.z - 10
// Visible range: view_z in [-100, -1]  =>  world z in [-90, 9].
// At view_z = -d, the frustum half-width = d (tan(45°) = 1).
ViewFrustum MakeTestFrustum() {
  Mat4f view = Mat4f::LookAtRH({0.f, 0.f, 10.f}, {0.f, 0.f, 0.f}, Vec3f::kAxisY);
  Mat4f proj = Mat4f::PerspectiveRH(kHalfPi, 1.f, 1.f, 100.f);
  // Column-vector convention: VP = proj * view.
  return ViewFrustum(proj * view);
}

}  // namespace

// ---- Construction -----------------------------------------------------------

TEST(ViewFrustumTest, ConstructsFromMatrix) {
  // Should not crash and produce 6 normalized plane normals.
  ViewFrustum f = MakeTestFrustum();
  for (int i = 0; i < 6; ++i) {
    auto fp = static_cast<ViewFrustum::FrustumPlane>(i);
    const float len = f.GetPlane(fp).GetNormal().Length();
    EXPECT_NEAR(len, 1.f, 1e-4f);
  }
}

TEST(ViewFrustumTest, CopyConstructor) {
  ViewFrustum a = MakeTestFrustum();
  ViewFrustum b = a;
  for (int i = 0; i < 6; ++i) {
    auto fp = static_cast<ViewFrustum::FrustumPlane>(i);
    EXPECT_NEAR(a.GetPlane(fp).GetDist(), b.GetPlane(fp).GetDist(), 1e-6f);
  }
}

// ---- ContainsPoint ----------------------------------------------------------

TEST(ViewFrustumTest, PointAtOriginIsInside) {
  // Origin: view_z = 0 - 10 = -10, inside [-100,-1].
  ViewFrustum f = MakeTestFrustum();
  EXPECT_TRUE(f.ContainsPoint({0.f, 0.f, 0.f}));
}

TEST(ViewFrustumTest, PointBehindCameraIsOutside) {
  // World z=15: view_z = 5, behind the near plane (positive = behind camera).
  ViewFrustum f = MakeTestFrustum();
  EXPECT_FALSE(f.ContainsPoint({0.f, 0.f, 15.f}));
}

TEST(ViewFrustumTest, PointBeyondFarPlaneIsOutside) {
  // World z=-200: view_z = -210, beyond far=100.
  ViewFrustum f = MakeTestFrustum();
  EXPECT_FALSE(f.ContainsPoint({0.f, 0.f, -200.f}));
}

TEST(ViewFrustumTest, PointFarLeftIsOutside) {
  // At view_z=-5 (world z=5), frustum half-width = 5.  x=-20 is outside.
  ViewFrustum f = MakeTestFrustum();
  EXPECT_FALSE(f.ContainsPoint({-20.f, 0.f, 5.f}));
}

TEST(ViewFrustumTest, PointInsideAtNonZeroXY) {
  // At view_z=-5 (world z=5), half-width=5: x=2, y=2 is inside.
  ViewFrustum f = MakeTestFrustum();
  EXPECT_TRUE(f.ContainsPoint({2.f, 2.f, 5.f}));
}

// ---- ContainsBBox -----------------------------------------------------------

TEST(ViewFrustumTest, BBoxAtOriginIsInside) {
  ViewFrustum f = MakeTestFrustum();
  BBox3 box(Vec3f{-0.5f, -0.5f, -0.5f}, Vec3f{0.5f, 0.5f, 0.5f});
  EXPECT_TRUE(f.ContainsBBox(box));
}

TEST(ViewFrustumTest, BBoxBeyondFarPlaneIsOutside) {
  ViewFrustum f = MakeTestFrustum();
  // Entirely beyond far (world z < -90): box at z=-300 to z=-200.
  BBox3 box(Vec3f{-1.f, -1.f, -300.f}, Vec3f{1.f, 1.f, -200.f});
  EXPECT_FALSE(f.ContainsBBox(box));
}

TEST(ViewFrustumTest, BBoxStraddlingFarPlaneIsInside) {
  ViewFrustum f = MakeTestFrustum();
  // Box from world z=-50 to z=-120: straddles far plane (z=-90), partially visible.
  BBox3 box(Vec3f{-1.f, -1.f, -120.f}, Vec3f{1.f, 1.f, -50.f});
  EXPECT_TRUE(f.ContainsBBox(box));
}

TEST(ViewFrustumTest, BBoxBehindCameraIsOutside) {
  ViewFrustum f = MakeTestFrustum();
  // Box entirely behind camera (world z > 9 means view_z > -1, before near plane).
  BBox3 box(Vec3f{-1.f, -1.f, 11.f}, Vec3f{1.f, 1.f, 20.f});
  EXPECT_FALSE(f.ContainsBBox(box));
}

TEST(ViewFrustumTest, BBoxStraddlingNearPlaneIsInside) {
  ViewFrustum f = MakeTestFrustum();
  // Box from world z=8 to z=5: part behind near (z>9), part inside.
  BBox3 box(Vec3f{-0.5f, -0.5f, 5.f}, Vec3f{0.5f, 0.5f, 11.f});
  EXPECT_TRUE(f.ContainsBBox(box));
}

// ---- ContainsLine -----------------------------------------------------------

TEST(ViewFrustumTest, LineFullyInsideIsContained) {
  ViewFrustum f = MakeTestFrustum();
  EXPECT_TRUE(f.ContainsLine({-1.f, 0.f, 0.f}, {1.f, 0.f, 0.f}));
}

TEST(ViewFrustumTest, LineFullyOutsideIsNotContained) {
  ViewFrustum f = MakeTestFrustum();
  // Both endpoints far behind the camera.
  EXPECT_FALSE(f.ContainsLine({0.f, 0.f, 20.f}, {0.f, 0.f, 50.f}));
}

TEST(ViewFrustumTest, LineCrossingFrustumIsContained) {
  ViewFrustum f = MakeTestFrustum();
  // From inside (0,0,0) to outside behind camera (0,0,30): entry inside → true.
  EXPECT_TRUE(f.ContainsLine({0.f, 0.f, 0.f}, {0.f, 0.f, 30.f}));
}

TEST(ViewFrustumTest, LineThroughFrustumBothEndsOutside) {
  ViewFrustum f = MakeTestFrustum();
  // From behind camera to beyond far: passes through the frustum.
  EXPECT_TRUE(f.ContainsLine({0.f, 0.f, 15.f}, {0.f, 0.f, -200.f}));
}
