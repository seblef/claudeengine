#pragma once

#include <string>
#include <vector>

#include "mesh/LodData.h"

namespace mesh {

// Geometry and material binding for one submesh.
struct SubmeshData {
  std::string material_slot;  // cppcheck-suppress unusedStructMember
  std::vector<LodData> lods;  // cppcheck-suppress unusedStructMember
};

}  // namespace mesh
