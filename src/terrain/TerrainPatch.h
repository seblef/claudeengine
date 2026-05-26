#pragma once

namespace terrain {

// Descriptor for a single renderable terrain patch selected by CDLODQuadTree.
//
// The renderer uses lod, grid_x, and grid_z to locate the patch in the LOD
// grid and compute the mesh world transform; morph is passed to the vertex
// shader to blend vertex positions toward the next coarser LOD.
struct TerrainPatch {
  // cppcheck-suppress unusedStructMember
  int   lod;     // LOD level (0 = finest)
  // cppcheck-suppress unusedStructMember
  int   grid_x;  // patch column in the LOD grid
  // cppcheck-suppress unusedStructMember
  int   grid_z;  // patch row in the LOD grid
  // cppcheck-suppress unusedStructMember
  float morph;   // blend factor [0, 1]: 0 = pure this LOD, 1 = next coarser
};

}  // namespace terrain
