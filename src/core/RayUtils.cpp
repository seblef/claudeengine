#include "core/RayUtils.h"

#include <cmath>

namespace core {

float RayTriangleIntersect(const Vec3f& orig, const Vec3f& dir,
                           const Vec3f& v0,   const Vec3f& v1,
                           const Vec3f& v2) {
  constexpr float kEpsilon = 1e-7f;
  const Vec3f e1 = v1 - v0;
  const Vec3f e2 = v2 - v0;
  const Vec3f h  = dir.Cross(e2);
  const float a  = e1.Dot(h);
  if (std::abs(a) < kEpsilon) return -1.f;
  const float f  = 1.f / a;
  const Vec3f s  = orig - v0;
  const float u  = f * s.Dot(h);
  if (u < 0.f || u > 1.f) return -1.f;
  const Vec3f q  = s.Cross(e1);
  const float v  = f * dir.Dot(q);
  if (v < 0.f || u + v > 1.f) return -1.f;
  const float t  = f * e2.Dot(q);
  return t >= 0.f ? t : -1.f;
}

Vec3f TransformPoint(const Mat4f& inv_world, const Vec3f& p) {
  return p * inv_world;
}

Vec3f TransformDir(const Mat4f& inv_world, const Vec3f& d) {
  return TransformNoTranslation(d, inv_world);
}

}  // namespace core
