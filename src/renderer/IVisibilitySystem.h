#pragma once

#include "core/BBox3.h"

namespace core { class ViewFrustum; }

namespace renderer {

class Renderable;

// Abstract interface for scene visibility systems.
//
// A visibility system owns a set of renderables and is responsible for
// deciding which of them are visible in a given frame (via CullAndEnqueue).
// Two implementations are provided:
//   - NoCullingVisibilitySystem: enqueues everything unconditionally.
//   - OctreeVisibilitySystem:    culls against a view frustum using an octree.
//
// Clients add/remove renderables through Renderer::AddRenderable /
// RemoveRenderable rather than calling these methods directly.
class IVisibilitySystem {
 public:
  explicit IVisibilitySystem(const core::BBox3& world_bounds)
      : world_bounds_(world_bounds) {}

  virtual ~IVisibilitySystem() = default;

  IVisibilitySystem(const IVisibilitySystem&)            = delete;
  IVisibilitySystem& operator=(const IVisibilitySystem&) = delete;

  // Registers a renderable with this system.
  // Sets r->visibility_system_ and r->visibility_id_ internally.
  virtual void AddRenderable(Renderable* r) = 0;

  // Unregisters a renderable previously added to this system.
  virtual void RemoveRenderable(Renderable* r) = 0;

  // Called by Renderable::SetWorldMatrix after the world bbox is updated.
  // Implementations may re-insert the renderable in the spatial structure.
  virtual void OnRenderableMoved(Renderable* r) = 0;

  // Determines visible renderables and calls Enqueue() on each of them.
  virtual void CullAndEnqueue(const core::ViewFrustum& frustum) = 0;

  // Removes all registered renderables from this system.
  virtual void Clear() = 0;

 protected:
  core::BBox3 world_bounds_;
};

}  // namespace renderer
