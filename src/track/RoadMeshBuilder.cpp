#include "track/RoadMeshBuilder.h"

#include <algorithm>
#include <cmath>

#include "core/Vec3f.h"
#include "track/RoadSpline.h"

namespace track {

// static
RoadMeshData RoadMeshBuilder::Build(
    const RoadSpline& spline,
    float width,
    float samples_per_metre,
    const std::function<float(float x, float z)>& height_fn) {
  RoadMeshData data;

  if (spline.GetPointCount() < 2)
    return data;

  const float approx_length = spline.GetApproximateLength();
  const int n = std::max(8, static_cast<int>(
      std::round(approx_length * samples_per_metre)));

  const float half_width = width * 0.5f;
  const float uv_scale   = approx_length / width;

  data.vertices.reserve(static_cast<size_t>(n) * 2 * 8);
  data.indices.reserve(static_cast<size_t>(n) * 6);

  static constexpr float kZFightOffset = 0.02f;
  static const core::Vec3f kUp{0.f, 1.f, 0.f};

  for (int i = 0; i < n; ++i) {
    const float t       = static_cast<float>(i) / static_cast<float>(n);
    const float u       = t * uv_scale;
    const core::Vec3f center  = spline.Sample(t);
    const core::Vec3f tangent = spline.GetTangent(t).Normalized();

    // right = Cross(tangent, up).Normalized() * half_width
    core::Vec3f right = tangent.Cross(kUp).Normalized();
    right = {right.x * half_width, right.y * half_width, right.z * half_width};

    const core::Vec3f left_pos  = center - right;
    const core::Vec3f right_pos = center + right;

    // Project to terrain height + z-fight offset.
    const float left_y = height_fn
        ? height_fn(left_pos.x, left_pos.z) + kZFightOffset
        : left_pos.y + kZFightOffset;
    const float right_y = height_fn
        ? height_fn(right_pos.x, right_pos.z) + kZFightOffset
        : right_pos.y + kZFightOffset;

    // Left vertex: pos(3) + normal(3) + uv(2)
    data.vertices.push_back(left_pos.x);
    data.vertices.push_back(left_y);
    data.vertices.push_back(left_pos.z);
    data.vertices.push_back(0.f);
    data.vertices.push_back(1.f);
    data.vertices.push_back(0.f);
    data.vertices.push_back(u);
    data.vertices.push_back(0.f);

    // Right vertex
    data.vertices.push_back(right_pos.x);
    data.vertices.push_back(right_y);
    data.vertices.push_back(right_pos.z);
    data.vertices.push_back(0.f);
    data.vertices.push_back(1.f);
    data.vertices.push_back(0.f);
    data.vertices.push_back(u);
    data.vertices.push_back(1.f);
  }

  // Connect adjacent cross-sections with two triangles each.
  // Last segment wraps back to sample 0 (looping road).
  for (int i = 0; i < n; ++i) {
    const uint32_t i0 = static_cast<uint32_t>(2 * i);
    const uint32_t i1 = static_cast<uint32_t>(2 * i + 1);
    const uint32_t i2 = static_cast<uint32_t>(2 * ((i + 1) % n));
    const uint32_t i3 = static_cast<uint32_t>(2 * ((i + 1) % n) + 1);

    data.indices.push_back(i0);
    data.indices.push_back(i2);
    data.indices.push_back(i1);

    data.indices.push_back(i1);
    data.indices.push_back(i2);
    data.indices.push_back(i3);
  }

  return data;
}

}  // namespace track
