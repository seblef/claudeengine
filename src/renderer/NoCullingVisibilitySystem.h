#pragma once

#include <vector>

#include "renderer/IVisibilitySystem.h"

namespace renderer {

// Visibility system that performs no culling: every registered renderable
// is enqueued unconditionally each frame.
//
// Intended for always-visible objects such as GlobalLight whose bounding box
// spans the entire world (BBox3::kInfinite) and must never be skipped.
//
// Removal and indexing are O(1) thanks to the swap-and-pop idiom: each
// renderable stores its current vector index in its visibility_id_ slot.
class NoCullingVisibilitySystem : public IVisibilitySystem {
 public:
  explicit NoCullingVisibilitySystem(const core::BBox3& world_bounds);

  // Calls Clear() so every registered renderable's system pointer is reset.
  ~NoCullingVisibilitySystem() override;

  // Appends r and stores its new index via r->SetVisibilityId().
  void AddRenderable(Renderable* r) override;

  // Swaps r with the last element, pops back, and updates the displaced
  // element's id.  O(1).
  void RemoveRenderable(Renderable* r) override;

  // No-op: flat list, position is irrelevant.
  void OnRenderableMoved(Renderable* r) override;

  // Calls Enqueue() on every registered renderable.  frustum is unused.
  void CullAndEnqueue(const core::ViewFrustum& frustum) override;

  // Clears all registrations and resets each renderable's system pointer.
  void Clear() override;

 private:
  // cppcheck-suppress unusedStructMember
  std::vector<Renderable*> renderables_;
};

}  // namespace renderer
