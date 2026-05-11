#pragma once

#include "core/Plane.h"
#include "core/Vec3f.h"

namespace core {

// Forward declarations — full types only needed in ViewFrustum.cpp.
class BBox3;
class Mat4f;

// Six-plane view frustum extracted from a combined view-projection matrix.
//
// Planes are stored in the order defined by FrustumPlane and their normals
// point inward: a point is visible if and only if it is on the front side (or
// on the boundary) of all six planes.
class ViewFrustum {
 public:
  // Index into the internal planes array.  The integer values are used
  // directly as array indices, so the order must not be changed.
  enum class FrustumPlane { kLeft = 0, kRight, kBottom, kTop, kNear, kFar };

  // ---- Construction --------------------------------------------------------

  ViewFrustum() = default;
  ViewFrustum(const ViewFrustum&) = default;
  ViewFrustum& operator=(const ViewFrustum&) = default;

  // Extracts the six frustum planes from a combined view-projection matrix
  // using the Gribb-Hartmann method.
  //
  // Convention: column-vector, clip = VP * p.  Let r0..r3 be the rows of vp:
  //   left   = r3 + r0,  right  = r3 - r0
  //   bottom = r3 + r1,  top    = r3 - r1
  //   near   = r3 + r2,  far    = r3 - r2   (OpenGL NDC: near z = -1)
  explicit ViewFrustum(const Mat4f& vp);

  // ---- Containment tests ---------------------------------------------------

  // Returns true if pt is inside or on the boundary of the frustum.
  [[nodiscard]] bool ContainsPoint(const Vec3f& pt) const;

  // Returns true if any part of the line segment [p1, p2] is inside the
  // frustum.  Uses Sutherland-Hodgman clipping against each plane.
  [[nodiscard]] bool ContainsLine(const Vec3f& p1, const Vec3f& p2) const;

  // Returns true if box is at least partially visible (not completely behind
  // any frustum plane).
  [[nodiscard]] bool ContainsBBox(const BBox3& box) const;

  // ---- Accessors -----------------------------------------------------------

  [[nodiscard]] const Plane& GetPlane(FrustumPlane p) const;

 private:
  Plane planes_[6];
};

}  // namespace core
