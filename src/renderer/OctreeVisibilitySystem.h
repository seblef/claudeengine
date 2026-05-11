#pragma once

#include <memory>
#include <vector>

#include "core/BBox3.h"
#include "renderer/IVisibilitySystem.h"

namespace renderer {

// Visibility system that culls renderables using a spatial octree.
//
// On CullAndEnqueue, the frustum is tested against each node's bounding box
// before descending into it.  Entire subtrees outside the frustum are skipped
// with a single ContainsBBox call, reducing Enqueue() calls to only the
// objects that can actually be seen.
//
// Insertion rule: each renderable is placed in the deepest node whose bounds
// fully contain the renderable's world_bbox_.  Objects that span multiple
// octants are stored at the deepest common ancestor.
//
// O(1) removal: the renderable's visibility_id_ stores the raw OctreeNode*
// it lives in; removal is a direct pointer cast + swap-and-pop.
class OctreeVisibilitySystem : public IVisibilitySystem {
 public:
  // Constructs an octree covering world_bounds with up to max_depth levels of
  // subdivision.  Use ComputeNumLevels() to choose an appropriate depth.
  OctreeVisibilitySystem(const core::BBox3& world_bounds, int max_depth);

  // Returns the recommended number of tree levels for a cubic world of the
  // given side length so that leaf cells are at least min_cell_size units wide.
  // Result is clamped to [1, 8].
  // Example: world_size=1000, min_cell_size=10 → 7 (leaf ~7.8 units).
  static int ComputeNumLevels(float world_size, float min_cell_size = 10.f);

  void AddRenderable(Renderable* r) override;
  void RemoveRenderable(Renderable* r) override;
  void OnRenderableMoved(Renderable* r) override;
  void CullAndEnqueue(const core::ViewFrustum& frustum) override;
  void Clear() override;

 private:
  struct OctreeNode {
    // cppcheck-suppress unusedStructMember
    core::BBox3 bounds;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<OctreeNode> children[8];  // null array = leaf
    // cppcheck-suppress unusedStructMember
    std::vector<Renderable*> renderables;

    bool IsLeaf() const { return children[0] == nullptr; }
  };

  // Inserts r into the deepest node in the subtree rooted at node that fully
  // contains r->GetWorldBBox().  Sets r->SetVisibilityId() to the chosen node.
  void Insert(OctreeNode* node, Renderable* r, int depth);

  // Removes r from node's renderables vector using swap-and-pop.
  void Remove(OctreeNode* node, Renderable* r);

  // Enqueues all renderables in visible nodes using DFS + frustum culling.
  void CullNode(const OctreeNode* node,
                const core::ViewFrustum& frustum) const;

  // Creates the 8 child nodes for node by halving its bounds.
  void SplitNode(OctreeNode* node) const;

  // Recursively clears all renderables and destroys children.
  void ClearNode(OctreeNode* node);

  // cppcheck-suppress unusedStructMember
  OctreeNode root_;
  // cppcheck-suppress unusedStructMember
  int        max_depth_;
};

}  // namespace renderer
