#pragma once

#include <cstdint>
#include <vector>

#include "core/BBox3.h"
#include "core/Vertex3D.h"
#include "mesh/SubMeshRange.h"

namespace mesh {

// Geometry data for one level of detail.
// When `submeshes` is non-empty each entry describes a contiguous index range
// that must be rendered with a distinct material.  An empty `submeshes` vector
// means legacy single-material behaviour — all existing code continues to work
// unchanged.
struct LodData {
  std::vector<core::Vertex3D> vertices;   // cppcheck-suppress unusedStructMember
  std::vector<uint32_t> indices;          // cppcheck-suppress unusedStructMember
  core::BBox3 aabb;                       // cppcheck-suppress unusedStructMember
  std::vector<SubMeshRange> submeshes;    // cppcheck-suppress unusedStructMember
};

}  // namespace mesh
