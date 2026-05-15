#pragma once

#include <algorithm>
#include <cmath>

#include "core/Mat4f.h"
#include "core/Vec3f.h"

namespace core {

// Axis-aligned 3D bounding box (AABB).
//
// Convention: min_ <= max_ on all axes for a valid box. Use IsValid() to check
// and Repair() to fix an inverted box. An uninitialized (default-constructed)
// box has indeterminate bounds — populate both min_ and max_ before use.
class BBox3 {
 public:
  // ---- Constants (defined after class — class must be complete) ---------------

  // Box spanning the entire representable float range on all axes.
  // Use for always-visible objects (e.g. GlobalLight) that bypass AABB culling.
  static const BBox3 kInfinite;

  // ---- Construction ----------------------------------------------------------

  BBox3() = default;
  BBox3(const BBox3&) = default;
  BBox3& operator=(const BBox3&) = default;

  inline BBox3(const Vec3f& min, const Vec3f& max) : min_(min), max_(max) {}

  inline BBox3(float xmin, float ymin, float zmin,
               float xmax, float ymax, float zmax)
      : min_(xmin, ymin, zmin), max_(xmax, ymax, zmax) {}

  // Returns a box centered at `center` with full extents `size`.
  [[nodiscard]] static inline BBox3 FromCenterAndSize(const Vec3f& center,
                                                      const Vec3f& size) {
    const Vec3f half = size * 0.5f;
    return {center - half, center + half};
  }

  // ---- Getters / Setters -----------------------------------------------------

  [[nodiscard]] inline const Vec3f& GetMin() const { return min_; }
  [[nodiscard]] inline const Vec3f& GetMax() const { return max_; }
  inline void SetMin(const Vec3f& min) { min_ = min; }
  inline void SetMax(const Vec3f& max) { max_ = max; }

  [[nodiscard]] inline Vec3f GetCenter() const { return (min_ + max_) * 0.5f; }
  [[nodiscard]] inline Vec3f GetSize()   const { return max_ - min_; }

  // ---- Translation -----------------------------------------------------------

  [[nodiscard]] inline BBox3 operator+(const Vec3f& v) const { return {min_ + v, max_ + v}; }
  inline BBox3& operator+=(const Vec3f& v) { min_ += v; max_ += v; return *this; }
  [[nodiscard]] inline BBox3 operator-(const Vec3f& v) const { return {min_ - v, max_ - v}; }
  inline BBox3& operator-=(const Vec3f& v) { min_ -= v; max_ -= v; return *this; }

  // ---- Validation ------------------------------------------------------------

  // Returns true if min <= max on every axis.
  [[nodiscard]] inline bool IsValid() const {
    return min_.x <= max_.x && min_.y <= max_.y && min_.z <= max_.z;
  }

  // Swaps each axis pair where min > max, making the box valid.
  inline void Repair() {
    if (min_.x > max_.x) std::swap(min_.x, max_.x);
    if (min_.y > max_.y) std::swap(min_.y, max_.y);
    if (min_.z > max_.z) std::swap(min_.z, max_.z);
  }

  // ---- Union (extend in-place) -----------------------------------------------

  // Extends this box to also contain `rhs`.
  inline BBox3& operator<<(const BBox3& rhs) {
    min_.x = std::min(min_.x, rhs.min_.x);
    min_.y = std::min(min_.y, rhs.min_.y);
    min_.z = std::min(min_.z, rhs.min_.z);
    max_.x = std::max(max_.x, rhs.max_.x);
    max_.y = std::max(max_.y, rhs.max_.y);
    max_.z = std::max(max_.z, rhs.max_.z);
    return *this;
  }

  // Extends this box to also contain point `v`.
  inline BBox3& operator<<(const Vec3f& v) {
    min_.x = std::min(min_.x, v.x);
    min_.y = std::min(min_.y, v.y);
    min_.z = std::min(min_.z, v.z);
    max_.x = std::max(max_.x, v.x);
    max_.y = std::max(max_.y, v.y);
    max_.z = std::max(max_.z, v.z);
    return *this;
  }

  // ---- Matrix transformation -------------------------------------------------

  // Transforms all 8 corners by `m` and returns the AABB of the results.
  [[nodiscard]] inline BBox3 operator*(const Mat4f& m) const {
    const Vec3f corners[8] = {
      {min_.x, min_.y, min_.z}, {max_.x, min_.y, min_.z},
      {min_.x, max_.y, min_.z}, {max_.x, max_.y, min_.z},
      {min_.x, min_.y, max_.z}, {max_.x, min_.y, max_.z},
      {min_.x, max_.y, max_.z}, {max_.x, max_.y, max_.z},
    };
    BBox3 result(corners[0] * m, corners[0] * m);
    for (int i = 1; i < 8; ++i) result << (corners[i] * m);
    return result;
  }

  inline BBox3& operator*=(const Mat4f& m) { *this = *this * m; return *this; }

  // ---- Overlap / Containment -------------------------------------------------

  // Returns true if the two AABBs overlap or touch on all three axes.
  [[nodiscard]] inline bool Overlaps(const BBox3& rhs) const {
    return min_.x <= rhs.max_.x && max_.x >= rhs.min_.x &&
           min_.y <= rhs.max_.y && max_.y >= rhs.min_.y &&
           min_.z <= rhs.max_.z && max_.z >= rhs.min_.z;
  }

  // Returns true if `v` is inside or on the boundary of this box.
  [[nodiscard]] inline bool Overlaps(const Vec3f& v) const {
    return v.x >= min_.x && v.x <= max_.x &&
           v.y >= min_.y && v.y <= max_.y &&
           v.z >= min_.z && v.z <= max_.z;
  }

  // Returns true if `rhs` is completely contained within this box.
  [[nodiscard]] inline bool IsCompletelyIn(const BBox3& rhs) const {
    return rhs.min_.x >= min_.x && rhs.max_.x <= max_.x &&
           rhs.min_.y >= min_.y && rhs.max_.y <= max_.y &&
           rhs.min_.z >= min_.z && rhs.max_.z <= max_.z;
  }

  // Returns true if `v` is inside this box shrunk inward by `radius` on all sides.
  [[nodiscard]] inline bool IsCompletelyIn(const Vec3f& v, float radius = 0.f) const {
    return v.x >= min_.x + radius && v.x <= max_.x - radius &&
           v.y >= min_.y + radius && v.y <= max_.y - radius &&
           v.z >= min_.z + radius && v.z <= max_.z - radius;
  }

  // ---- Distance --------------------------------------------------------------

  // Returns the squared distance from point `p` to the nearest point on (or in) this box.
  [[nodiscard]] inline float SquaredDistance(const Vec3f& p) const {
    auto axis_sq = [](float v, float lo, float hi) -> float {
      if (v < lo) {
        const float d = lo - v;
        return d * d;
      }
      if (v > hi) {
        const float d = v - hi;
        return d * d;
      }
      return 0.f;
    };
    return axis_sq(p.x, min_.x, max_.x) +
           axis_sq(p.y, min_.y, max_.y) +
           axis_sq(p.z, min_.z, max_.z);
  }

  [[nodiscard]] inline float Distance(const Vec3f& p) const {
    return std::sqrt(SquaredDistance(p));
  }

  // ---- Ray intersection (slab method) ----------------------------------------

  // Returns true if the ray (origin, dir) intersects this box.
  // `dir` must be normalized. The box is treated as solid (intersection from
  // inside also returns true).
  [[nodiscard]] inline bool IntersectsRay(const Vec3f& origin,
                                          const Vec3f& dir) const {
    // Use scalar reciprocals to avoid division in the loop and to handle the
    // sign correctly. Division by zero (axis-aligned rays) yields ±inf, which
    // produces correct slab min/max via std::min/max.
    const float inv_x = 1.f / dir.x;
    const float inv_y = 1.f / dir.y;
    const float inv_z = 1.f / dir.z;

    const float tx1 = (min_.x - origin.x) * inv_x;
    const float tx2 = (max_.x - origin.x) * inv_x;
    const float ty1 = (min_.y - origin.y) * inv_y;
    const float ty2 = (max_.y - origin.y) * inv_y;
    const float tz1 = (min_.z - origin.z) * inv_z;
    const float tz2 = (max_.z - origin.z) * inv_z;

    const float t_enter = std::max({std::min(tx1, tx2),
                                    std::min(ty1, ty2),
                                    std::min(tz1, tz2)});
    const float t_exit  = std::min({std::max(tx1, tx2),
                                    std::max(ty1, ty2),
                                    std::max(tz1, tz2)});

    return t_enter <= t_exit && t_exit >= 0.f;
  }

  // Same as IntersectsRay but also writes the t of the nearest hit to t_out.
  // If the ray origin is inside the box, t_out is set to 0. Returns false and
  // leaves t_out unchanged when the ray misses.
  [[nodiscard]] inline bool IntersectsRay(const Vec3f& origin, const Vec3f& dir,
                                          float& t_out) const {
    const float inv_x = 1.f / dir.x;
    const float inv_y = 1.f / dir.y;
    const float inv_z = 1.f / dir.z;

    const float tx1 = (min_.x - origin.x) * inv_x;
    const float tx2 = (max_.x - origin.x) * inv_x;
    const float ty1 = (min_.y - origin.y) * inv_y;
    const float ty2 = (max_.y - origin.y) * inv_y;
    const float tz1 = (min_.z - origin.z) * inv_z;
    const float tz2 = (max_.z - origin.z) * inv_z;

    const float t_enter = std::max({std::min(tx1, tx2),
                                    std::min(ty1, ty2),
                                    std::min(tz1, tz2)});
    const float t_exit  = std::min({std::max(tx1, tx2),
                                    std::max(ty1, ty2),
                                    std::max(tz1, tz2)});

    if (t_enter <= t_exit && t_exit >= 0.f) {
      t_out = (t_enter >= 0.f) ? t_enter : 0.f;
      return true;
    }
    return false;
  }

 private:
  Vec3f min_;
  Vec3f max_;
};

inline const BBox3 BBox3::kInfinite{
    {-1e30f, -1e30f, -1e30f}, {1e30f, 1e30f, 1e30f}};

}  // namespace core
