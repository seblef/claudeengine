#pragma once

#include <cstdint>
#include <vector>

#include "core/BBox3.h"
#include "core/Vertex3D.h"

namespace mesh {

// Geometry data for one level of detail.
struct LodData {
  std::vector<core::Vertex3D> vertices;  // cppcheck-suppress unusedStructMember
  std::vector<uint32_t> indices;         // cppcheck-suppress unusedStructMember
  core::BBox3 aabb;                      // cppcheck-suppress unusedStructMember
};

}  // namespace mesh
