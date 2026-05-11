#pragma once

#include <cmath>

#include "core/Vec3f.h"

namespace core {

// Forward declarations — method bodies using these types live in Plane.cpp.
class BBox3;
class Mat4f;

// Infinite plane in 3D space, defined by a unit normal and a signed distance
// from the origin.  The plane equation is: normal · p = dist.
//
// A point p is on the front side when normal·p > dist, on the back side when
// normal·p < dist, and on the plane when normal·p == dist.
class Plane {
 public:
  // Classification of a point or bounding box relative to the plane.
  enum class Side { kBack, kFront, kOn, kClip };

  // ---- Constants (defined after class — class must be complete) ------------

  // YZ plane (x = 0), normal pointing toward +X.
  static const Plane kAxisX;
  // XZ plane (y = 0), normal pointing toward +Y.
  static const Plane kAxisY;
  // XY plane (z = 0), normal pointing toward +Z.
  static const Plane kAxisZ;

  // ---- Construction --------------------------------------------------------

  Plane() = default;
  Plane(const Plane&) = default;
  Plane& operator=(const Plane&) = default;

  // Constructs from three points.  The normal is determined by CCW winding:
  // n = normalize((p2-p1) × (p3-p1)).
  Plane(const Vec3f& p1, const Vec3f& p2, const Vec3f& p3);

  // Constructs from a point on the plane and the plane normal.
  // The normal is normalized internally.
  Plane(const Vec3f& point, const Vec3f& normal);

  // Constructs from a signed distance from the origin and a plane normal.
  // The normal is normalized internally.
  Plane(float dist, const Vec3f& normal);

  // ---- Accessors -----------------------------------------------------------

  [[nodiscard]] inline const Vec3f& GetNormal() const { return normal_; }
  [[nodiscard]] inline float        GetDist()   const { return dist_; }

  // ---- Classification ------------------------------------------------------

  // Returns which side of the plane point pt lies on.
  [[nodiscard]] inline Side Classify(const Vec3f& pt) const {
    const float d = normal_.Dot(pt) - dist_;
    if (d > kEps)  return Side::kFront;
    if (d < -kEps) return Side::kBack;
    return Side::kOn;
  }

  // Returns the classification of an axis-aligned bounding box against the
  // plane.  kClip means the plane intersects the box interior.
  [[nodiscard]] Side Classify(const BBox3& box) const;

  // ---- Intersection --------------------------------------------------------

  // Ray/plane intersection.  dir need not be normalized.
  // Returns false when the ray is parallel to the plane (no intersection).
  // out_dist is the signed parameter t such that origin + t*dir = intersection.
  [[nodiscard]] bool IntersectsLine(const Vec3f& origin, const Vec3f& dir,
                                    float& out_dist) const;

  // Ray/plane intersection returning the world-space hit point.
  [[nodiscard]] bool IntersectsLine(const Vec3f& origin, const Vec3f& dir,
                                    Vec3f& out_point) const;

  // Line-segment (edge) intersection.
  // Returns false when the edge is parallel to the plane or does not cross it
  // within the interval [p1, p2] (t outside [0, 1]).
  [[nodiscard]] bool IntersectsEdge(const Vec3f& p1, const Vec3f& p2,
                                    Vec3f& out_point) const;

  // ---- Transform -----------------------------------------------------------

  // Returns the plane transformed by matrix m.
  // Points transform as p' = m * p; planes transform by the inverse-transpose:
  // new_plane = m.Inverse().Transpose() * (normal, -dist).
  [[nodiscard]] Plane operator*(const Mat4f& m) const;

 private:
  static constexpr float kEps = 1e-5f;

  Vec3f normal_;
  float dist_ = 0.f;
};

inline const Plane Plane::kAxisX{0.f, Vec3f{1.f, 0.f, 0.f}};
inline const Plane Plane::kAxisY{0.f, Vec3f{0.f, 1.f, 0.f}};
inline const Plane Plane::kAxisZ{0.f, Vec3f{0.f, 0.f, 1.f}};

}  // namespace core
