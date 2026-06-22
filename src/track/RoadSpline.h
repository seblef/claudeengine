#pragma once

#include <vector>

#include "core/Vec3f.h"

namespace track {

// Looping Catmull-Rom spline over a set of control points.
//
// Uniform parameterisation: t in [0, 1) maps to the full loop across all
// control points. Each segment covers exactly 1/N of the t range, where N is
// the number of control points.
//
// Requires at least 2 control points for any query; undefined with fewer.
class RoadSpline {
 public:
  void AddControlPoint(const core::Vec3f& p);
  void SetControlPoint(int index, const core::Vec3f& p);
  void RemoveControlPoint(int index);
  void InsertControlPoint(int after_index, const core::Vec3f& p);

  [[nodiscard]] int         GetPointCount()          const;
  [[nodiscard]] core::Vec3f GetControlPoint(int index) const;

  // World-space position on the spline at parameter t in [0, 1).
  [[nodiscard]] core::Vec3f Sample(float t)    const;

  // World-space tangent (unnormalised) at parameter t in [0, 1).
  [[nodiscard]] core::Vec3f GetTangent(float t) const;

  // Approximate arc length computed as the chord-sum of 200 uniform samples.
  [[nodiscard]] float GetApproximateLength() const;

 private:
  // Evaluates the Catmull-Rom segment defined by p0…p3 at local parameter s.
  static core::Vec3f CatmullRom(const core::Vec3f& p0,
                                 const core::Vec3f& p1,
                                 const core::Vec3f& p2,
                                 const core::Vec3f& p3,
                                 float s);

  // Returns the derivative of the Catmull-Rom segment at local parameter s.
  static core::Vec3f CatmullRomTangent(const core::Vec3f& p0,
                                        const core::Vec3f& p1,
                                        const core::Vec3f& p2,
                                        const core::Vec3f& p3,
                                        float s);

  // Decomposes t into segment index and local parameter s in [0, 1).
  // segment is in [0, N) and s = t * N - segment.
  void Decompose(float t, int* segment, float* s) const;

  // cppcheck-suppress unusedStructMember
  std::vector<core::Vec3f> control_points_;
};

}  // namespace track
