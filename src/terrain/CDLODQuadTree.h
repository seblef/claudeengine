#pragma once

#include <vector>

#include "core/BBox3.h"
#include "terrain/TerrainPatch.h"

namespace core {
class Camera;
class ViewFrustum;
}  // namespace core

namespace terrain {

class TerrainData;

// Continuous Distance-Dependent Level of Detail (CDLOD) quadtree.
//
// Build() pre-computes a flat array of quadtree nodes from terrain dimensions.
// Select() performs frustum culling and per-node LOD selection driven by a
// configurable triangle budget, then fills a list of TerrainPatch descriptors
// ready for GPU submission.
//
// All nodes are stored in a flat std::vector (children referenced by index)
// to avoid heap fragmentation.
class CDLODQuadTree {
 public:
  // Builds the quadtree from terrain data.
  //
  // patch_size — number of quads per patch side (at LOD 0)
  // lod_count  — total number of LOD levels (LOD 0 is finest)
  void Build(const TerrainData& data, int patch_size, int lod_count);

  // Selects visible patches for `camera` within a `max_triangles` budget.
  //
  // LOD switch distances are derived from `max_triangles` so that the total
  // rendered triangle count stays approximately within budget.  `out` is
  // cleared and refilled on each call.
  void Select(const core::Camera& camera, int max_triangles,
              std::vector<TerrainPatch>& out) const;

 private:
  struct Node {
    // cppcheck-suppress unusedStructMember
    core::BBox3 aabb;
    // cppcheck-suppress unusedStructMember
    int         lod;
    // cppcheck-suppress unusedStructMember
    int         grid_x;
    // cppcheck-suppress unusedStructMember
    int         grid_z;
    // Indices into nodes_.  -1 means no child (leaf or terrain-edge clamp).
    // cppcheck-suppress unusedStructMember
    int         children[4];
  };

  // Recursively allocates nodes for the subtree rooted at (lod, grid_x, grid_z)
  // and returns the new node's index in nodes_.
  int BuildNode(const TerrainData& data, int lod, int grid_x, int grid_z);

  // Traverses the subtree at `idx`, culling by `frustum` and selecting LOD
  // based on distance from `cam_pos` to the node AABB.
  void SelectNode(int idx, const core::ViewFrustum& frustum,
                  const core::Vec3f& cam_pos, const float* lod_ranges,
                  std::vector<TerrainPatch>& out) const;

  // cppcheck-suppress unusedStructMember
  std::vector<Node> nodes_;
  // cppcheck-suppress unusedStructMember
  std::vector<int>  roots_;         // indices of top-level (coarsest) nodes
  int   patch_size_       = 0;
  int   lod_count_        = 0;
  float meters_per_texel_ = 1.f;
};

}  // namespace terrain
