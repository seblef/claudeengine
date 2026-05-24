#pragma once

#include "core/Mat4f.h"
#include "core/Vec3f.h"

namespace core {

// Möller-Trumbore ray-triangle intersection.
// Returns t >= 0 on hit, -1.f on miss. The t value is in the same space as
// orig/dir (world or model depending on the caller).
[[nodiscard]] float RayTriangleIntersect(const Vec3f& orig, const Vec3f& dir,
                                         const Vec3f& v0,   const Vec3f& v1,
                                         const Vec3f& v2);

// Transform a world-space point into the space defined by inv_world
// (includes translation).
[[nodiscard]] Vec3f TransformPoint(const Mat4f& inv_world, const Vec3f& p);

// Transform a world-space direction into the space defined by inv_world
// (no translation — upper-left 3×3 only).
[[nodiscard]] Vec3f TransformDir(const Mat4f& inv_world, const Vec3f& d);

}  // namespace core
