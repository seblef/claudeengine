#pragma once

#include <vector>

#include "mesh/SubmeshData.h"

namespace mesh {

// Top-level mesh asset: a collection of submeshes, each with one or more LODs.
// All submeshes must carry the same number of LODs.
struct MeshData {
  std::vector<SubmeshData> submeshes;  // cppcheck-suppress unusedStructMember
};

}  // namespace mesh
