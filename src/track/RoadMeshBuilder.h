#pragma once

#include <cstdint>
#include <functional>
#include <vector>

namespace track {

class RoadSpline;

// Raw mesh data for a road ribbon: interleaved vertex buffer and index buffer.
//
// Vertex layout per vertex (8 floats):
//   position (3) | normal (3) | uv (2)
// Normal is always (0, 1, 0) (up).
// UV: u tiles along the road, v is 0 (left edge) or 1 (right edge).
// Indices form triangle pairs connecting adjacent cross-sections.
struct RoadMeshData {
  // cppcheck-suppress unusedStructMember
  std::vector<float>    vertices;  // interleaved: position(3) + normal(3) + uv(2)
  // cppcheck-suppress unusedStructMember
  std::vector<uint32_t> indices;   // triangles
};

// Builds a flat ribbon mesh along a RoadSpline with optional terrain draping.
class RoadMeshBuilder {
 public:
  // Builds a ribbon mesh along spline.
  //
  // width:             road width in metres.
  // samples_per_metre: mesh density along the spline (default 1.0).
  // height_fn:         returns terrain Y at world (x, z); may be nullptr (flat
  //                    road with Y taken from spline position).
  [[nodiscard]] static RoadMeshData Build(
      const RoadSpline& spline,
      float width,
      float samples_per_metre,
      const std::function<float(float x, float z)>& height_fn);
};

}  // namespace track
