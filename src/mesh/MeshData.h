#pragma once

#include "mesh/LodData.h"

namespace mesh {

// Top-level mesh asset: a single flat geometry (one LOD, no submeshes).
// Materials are a game-level concept and are not stored here.
struct MeshData {
  LodData lod;  // cppcheck-suppress unusedStructMember
};

}  // namespace mesh
