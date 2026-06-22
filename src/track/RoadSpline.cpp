#include "track/RoadSpline.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace track {

void RoadSpline::AddControlPoint(const core::Vec3f& p) {
  control_points_.push_back(p);
}

void RoadSpline::SetControlPoint(int index, const core::Vec3f& p) {
  assert(index >= 0 && index < static_cast<int>(control_points_.size()));
  control_points_[index] = p;
}

void RoadSpline::RemoveControlPoint(int index) {
  assert(index >= 0 && index < static_cast<int>(control_points_.size()));
  control_points_.erase(control_points_.begin() + index);
}

void RoadSpline::InsertControlPoint(int after_index, const core::Vec3f& p) {
  const int pos = after_index + 1;
  assert(pos >= 0 && pos <= static_cast<int>(control_points_.size()));
  control_points_.insert(control_points_.begin() + pos, p);
}

int RoadSpline::GetPointCount() const {
  return static_cast<int>(control_points_.size());
}

core::Vec3f RoadSpline::GetControlPoint(int index) const {
  assert(index >= 0 && index < static_cast<int>(control_points_.size()));
  return control_points_[index];
}

core::Vec3f RoadSpline::Sample(float t) const {
  int seg;
  float s;
  Decompose(t, &seg, &s);

  const int n = static_cast<int>(control_points_.size());
  const core::Vec3f& p0 = control_points_[(seg - 1 + n) % n];
  const core::Vec3f& p1 = control_points_[seg];
  const core::Vec3f& p2 = control_points_[(seg + 1) % n];
  const core::Vec3f& p3 = control_points_[(seg + 2) % n];
  return CatmullRom(p0, p1, p2, p3, s);
}

core::Vec3f RoadSpline::GetTangent(float t) const {
  int seg;
  float s;
  Decompose(t, &seg, &s);

  const int n = static_cast<int>(control_points_.size());
  const core::Vec3f& p0 = control_points_[(seg - 1 + n) % n];
  const core::Vec3f& p1 = control_points_[seg];
  const core::Vec3f& p2 = control_points_[(seg + 1) % n];
  const core::Vec3f& p3 = control_points_[(seg + 2) % n];
  return CatmullRomTangent(p0, p1, p2, p3, s);
}

float RoadSpline::GetApproximateLength() const {
  if (control_points_.size() < 2) return 0.f;

  constexpr int kSamples = 200;
  float length = 0.f;
  core::Vec3f prev = Sample(0.f);
  for (int i = 1; i <= kSamples; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kSamples);
    const core::Vec3f cur = Sample(t);
    const core::Vec3f diff = cur - prev;
    length += std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    prev = cur;
  }
  return length;
}

// static
core::Vec3f RoadSpline::CatmullRom(const core::Vec3f& p0,
                                    const core::Vec3f& p1,
                                    const core::Vec3f& p2,
                                    const core::Vec3f& p3,
                                    float s) {
  const float s2 = s * s;
  const float s3 = s2 * s;
  const float c0 =  2.f * p1.x;
  const float c1 = -p0.x + p2.x;
  const float c2 =  2.f * p0.x - 5.f * p1.x + 4.f * p2.x - p3.x;
  const float c3 = -p0.x + 3.f * p1.x - 3.f * p2.x + p3.x;
  const float x  = 0.5f * (c0 + c1 * s + c2 * s2 + c3 * s3);

  const float d0 =  2.f * p1.y;
  const float d1 = -p0.y + p2.y;
  const float d2 =  2.f * p0.y - 5.f * p1.y + 4.f * p2.y - p3.y;
  const float d3 = -p0.y + 3.f * p1.y - 3.f * p2.y + p3.y;
  const float y  = 0.5f * (d0 + d1 * s + d2 * s2 + d3 * s3);

  const float e0 =  2.f * p1.z;
  const float e1 = -p0.z + p2.z;
  const float e2 =  2.f * p0.z - 5.f * p1.z + 4.f * p2.z - p3.z;
  const float e3 = -p0.z + 3.f * p1.z - 3.f * p2.z + p3.z;
  const float z  = 0.5f * (e0 + e1 * s + e2 * s2 + e3 * s3);

  return {x, y, z};
}

// static
core::Vec3f RoadSpline::CatmullRomTangent(const core::Vec3f& p0,
                                           const core::Vec3f& p1,
                                           const core::Vec3f& p2,
                                           const core::Vec3f& p3,
                                           float s) {
  // Derivative of CatmullRom wrt s, scaled by 0.5.
  const float s2 = s * s;

  const float c1 = -p0.x + p2.x;
  const float c2 =  2.f * (2.f * p0.x - 5.f * p1.x + 4.f * p2.x - p3.x);
  const float c3 =  3.f * (-p0.x + 3.f * p1.x - 3.f * p2.x + p3.x);
  const float x  = 0.5f * (c1 + c2 * s + c3 * s2);

  const float d1 = -p0.y + p2.y;
  const float d2 =  2.f * (2.f * p0.y - 5.f * p1.y + 4.f * p2.y - p3.y);
  const float d3 =  3.f * (-p0.y + 3.f * p1.y - 3.f * p2.y + p3.y);
  const float y  = 0.5f * (d1 + d2 * s + d3 * s2);

  const float e1 = -p0.z + p2.z;
  const float e2 =  2.f * (2.f * p0.z - 5.f * p1.z + 4.f * p2.z - p3.z);
  const float e3 =  3.f * (-p0.z + 3.f * p1.z - 3.f * p2.z + p3.z);
  const float z  = 0.5f * (e1 + e2 * s + e3 * s2);

  return {x, y, z};
}

void RoadSpline::Decompose(float t, int* segment, float* s) const {
  const int n = static_cast<int>(control_points_.size());
  const float ft = t * static_cast<float>(n);
  *segment = static_cast<int>(ft);
  // Clamp to valid segment range.
  if (*segment >= n) *segment = n - 1;
  *s = ft - static_cast<float>(*segment);
}

}  // namespace track
