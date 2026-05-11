#include "core/Plane.h"

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/Vec4f.h"

namespace core {

Plane::Plane(const Vec3f& p1, const Vec3f& p2, const Vec3f& p3)
    : normal_((p2 - p1).Cross(p3 - p1).Normalized()),
      dist_(normal_.Dot(p1)) {}

Plane::Plane(const Vec3f& point, const Vec3f& normal)
    : normal_(normal.Normalized()),
      dist_(normal_.Dot(point)) {}

Plane::Plane(float dist, const Vec3f& normal)
    : normal_(normal.Normalized()),
      dist_(dist) {}

Plane::Side Plane::Classify(const BBox3& box) const {
  const Vec3f& mn = box.GetMin();
  const Vec3f& mx = box.GetMax();
  const Vec3f corners[8] = {
    {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
    {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z},
    {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
    {mn.x, mx.y, mx.z}, {mx.x, mx.y, mx.z},
  };
  bool any_front = false;
  bool any_back  = false;
  for (const Vec3f& c : corners) {
    const float d = normal_.Dot(c) - dist_;
    if (d > kEps)       any_front = true;
    else if (d < -kEps) any_back  = true;
    if (any_front && any_back) return Side::kClip;
  }
  if (any_front) return Side::kFront;
  if (any_back)  return Side::kBack;
  return Side::kOn;
}

bool Plane::IntersectsLine(const Vec3f& origin, const Vec3f& dir,
                           float& out_dist) const {
  const float denom = normal_.Dot(dir);
  if (std::fabs(denom) < kEps) return false;
  out_dist = (dist_ - normal_.Dot(origin)) / denom;
  return true;
}

bool Plane::IntersectsLine(const Vec3f& origin, const Vec3f& dir,
                           Vec3f& out_point) const {
  float t;
  if (!IntersectsLine(origin, dir, t)) return false;
  out_point = origin + dir * t;
  return true;
}

bool Plane::IntersectsEdge(const Vec3f& p1, const Vec3f& p2,
                           Vec3f& out_point) const {
  const Vec3f edge  = p2 - p1;
  const float denom = normal_.Dot(edge);
  if (std::fabs(denom) < kEps) return false;
  const float t = (dist_ - normal_.Dot(p1)) / denom;
  if (t < 0.f || t > 1.f) return false;
  out_point = p1 + edge * t;
  return true;
}

Plane Plane::operator*(const Mat4f& m) const {
  const Vec4f plane_vec(normal_.x, normal_.y, normal_.z, -dist_);
  const Vec4f t = plane_vec * m.Inverse().Transpose();
  const Vec3f n(t.x, t.y, t.z);
  const float len = n.Length();
  return Plane(-t.w / len, n * (1.f / len));
}

}  // namespace core
