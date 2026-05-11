#include "core/ViewFrustum.h"

#include <algorithm>

#include "core/BBox3.h"
#include "core/Mat4f.h"

namespace core {

namespace {

// Builds a Plane from a raw 4-vector (a,b,c,d) extracted by Gribb-Hartmann,
// where the plane equation for visible points is: a*x + b*y + c*z + d >= 0.
// Converts to Plane storage format (normal · p = dist, kFront = visible):
//   normal = (a,b,c)/len,  dist = -d/len
Plane PlaneFromGH(float a, float b, float c, float d) {
  const float len = std::sqrt(a * a + b * b + c * c);
  return Plane(-d / len, Vec3f{a / len, b / len, c / len});
}

}  // namespace

ViewFrustum::ViewFrustum(const Mat4f& vp) {
  // Rows of vp: row i is (vp(i,0), vp(i,1), vp(i,2), vp(i,3)).
  const int kL = static_cast<int>(FrustumPlane::kLeft);
  const int kR = static_cast<int>(FrustumPlane::kRight);
  const int kB = static_cast<int>(FrustumPlane::kBottom);
  const int kT = static_cast<int>(FrustumPlane::kTop);
  const int kN = static_cast<int>(FrustumPlane::kNear);
  const int kF = static_cast<int>(FrustumPlane::kFar);

  // Left:   r3 + r0
  planes_[kL] = PlaneFromGH(vp(3,0)+vp(0,0), vp(3,1)+vp(0,1),
                             vp(3,2)+vp(0,2), vp(3,3)+vp(0,3));
  // Right:  r3 - r0
  planes_[kR] = PlaneFromGH(vp(3,0)-vp(0,0), vp(3,1)-vp(0,1),
                             vp(3,2)-vp(0,2), vp(3,3)-vp(0,3));
  // Bottom: r3 + r1
  planes_[kB] = PlaneFromGH(vp(3,0)+vp(1,0), vp(3,1)+vp(1,1),
                             vp(3,2)+vp(1,2), vp(3,3)+vp(1,3));
  // Top:    r3 - r1
  planes_[kT] = PlaneFromGH(vp(3,0)-vp(1,0), vp(3,1)-vp(1,1),
                             vp(3,2)-vp(1,2), vp(3,3)-vp(1,3));
  // Near:   r3 + r2  (OpenGL NDC: near z = -1)
  planes_[kN] = PlaneFromGH(vp(3,0)+vp(2,0), vp(3,1)+vp(2,1),
                             vp(3,2)+vp(2,2), vp(3,3)+vp(2,3));
  // Far:    r3 - r2
  planes_[kF] = PlaneFromGH(vp(3,0)-vp(2,0), vp(3,1)-vp(2,1),
                             vp(3,2)-vp(2,2), vp(3,3)-vp(2,3));
}

bool ViewFrustum::ContainsPoint(const Vec3f& pt) const {
  return std::none_of(planes_, planes_ + 6,
      [&](const Plane& p) { return p.Classify(pt) == Plane::Side::kBack; });
}

bool ViewFrustum::ContainsLine(const Vec3f& p1, const Vec3f& p2) const {
  // Sutherland-Hodgman segment clipping against each frustum half-space.
  // The segment [start, end] is progressively clipped to the front side of
  // each plane.  If both endpoints end up on the back side of any plane the
  // segment is entirely outside the frustum.
  Vec3f start = p1;
  Vec3f end   = p2;
  for (const Plane& plane : planes_) {
    const bool s_in = plane.Classify(start) != Plane::Side::kBack;
    const bool e_in = plane.Classify(end)   != Plane::Side::kBack;
    if (!s_in && !e_in) return false;
    if (!s_in) {
      Vec3f hit;
      if (plane.IntersectsEdge(start, end, hit)) start = hit;
    } else if (!e_in) {
      Vec3f hit;
      if (plane.IntersectsEdge(start, end, hit)) end = hit;
    }
  }
  return true;
}

bool ViewFrustum::ContainsBBox(const BBox3& box) const {
  return std::none_of(planes_, planes_ + 6,
      [&](const Plane& p) { return p.Classify(box) == Plane::Side::kBack; });
}

const Plane& ViewFrustum::GetPlane(FrustumPlane p) const {
  return planes_[static_cast<int>(p)];
}

}  // namespace core
