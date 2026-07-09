#pragma once

#include <cstdint>
#include <vector>

#include "core/Vertex3D.h"

namespace track {

// Raw mesh data for a flat rectangular tile quad: 4 vertices, 2 triangles.
struct TileMeshData {
  // cppcheck-suppress unusedStructMember
  std::vector<core::Vertex3D> vertices;
  // cppcheck-suppress unusedStructMember
  std::vector<uint32_t>       indices;
};

// Builds the renderable footprint of a TerrainTile: a flat quad in local
// space, centred at the origin, lying in the XZ plane (Y = 0) with an
// upward (+Y) normal.
//
// The quad never follows terrain curvature — it is posed flat by the owning
// GameObject's world transform, snapped once to the terrain height at
// placement time. This matches the "simplest possible hazard primitive"
// intent: a surface-property override under a flat pad, not a terrain
// modification.
class TerrainTile {
 public:
  // width:  local X extent, metres.
  // length: local Z extent, metres.
  [[nodiscard]] static TileMeshData BuildQuad(float width, float length);
};

}  // namespace track
