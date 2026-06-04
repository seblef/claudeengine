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
  Vec3f near_(box.GetMax());
  Vec3f far_(box.GetMin());

  if(normal_.x > 0.0f)  std::swap(near_.x, far_.x);
  if(normal_.y > 0.0f)  std::swap(near_.y,far_.y);
  if(normal_.z > 0.0f)  std::swap(near_.z,far_.z);

  const float dnear = normal_.Dot(near_) - dist_;
  if (dnear > kEps) return Side::kFront;

  const float dfar = normal_.Dot(far_) - dist_;
  if (dfar > kEps) return Side::kClip;

  // Both extremes within epsilon of the plane: box lies entirely on it.
  if (dfar >= -kEps && dnear >= -kEps) return Side::kOn;
  return Side::kBack;
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
