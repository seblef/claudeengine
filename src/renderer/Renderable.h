#pragma once

#include <cstdint>

#include "core/BBox3.h"
#include "core/Mat4f.h"

namespace renderer {

class IVisibilitySystem;  // forward declaration — full type not needed here

// Abstract base for all renderable objects (meshes, actors, lights, etc.).
//
// Maintains a world matrix, a local bounding box (immutable after
// construction), and a world bounding box (recomputed whenever the world
// matrix changes). Records the last frame the object was enqueued to support
// frame-based visibility culling.
class Renderable {
 public:
  // Constructs from a local bbox, initial world matrix, and visibility flag.
  // world_bbox is computed immediately as local_bbox * world_matrix.
  Renderable(const core::BBox3& local_bbox,
             const core::Mat4f& world_matrix,
             bool always_visible);

  // Copy-constructs from another renderable (copies all spatial data).
  explicit Renderable(const Renderable* other);

  virtual ~Renderable() = default;

  Renderable(const Renderable&)            = delete;
  Renderable& operator=(const Renderable&) = delete;

  // Enqueues this object for rendering in the current frame.
  virtual void Enqueue() = 0;

  // Returns true if this object should cast shadows.  Defaults to false; only
  // mesh-based renderables that belong to shadow-casting materials override this.
  [[nodiscard]] virtual bool IsShadowCaster() const { return false; }

  // Enqueues this object into the appropriate renderer's depth queue for the
  // current shadow pass.  No-op by default; mesh-based renderables override
  // this to call ObjectRenderer::AddDepthInstance.
  virtual void EnqueueDepth() {}

  [[nodiscard]] const core::Mat4f& GetWorldMatrix() const;

  // Sets the world matrix and recomputes the world bounding box.
  void SetWorldMatrix(const core::Mat4f& m);

  [[nodiscard]] const core::BBox3& GetLocalBBox()  const;
  [[nodiscard]] const core::BBox3& GetWorldBBox()  const;

  [[nodiscard]] uint64_t GetLastFrame() const;
  void                        SetLastFrame(uint64_t frame);

  [[nodiscard]] bool IsAlwaysVisible() const;

  // ---- Visibility system slot -----------------------------------------------
  //
  // visibility_id_ is an opaque handle stored by the owning IVisibilitySystem:
  //   NoCullingVisibilitySystem — vector index (enables O(1) swap+pop removal)
  //   OctreeVisibilitySystem    — OctreeNode* (enables O(1) direct node access)

  void SetVisibilitySystem(IVisibilitySystem* s);
  [[nodiscard]] IVisibilitySystem* GetVisibilitySystem() const;

  void          SetVisibilityId(uintptr_t id);
  [[nodiscard]] uintptr_t GetVisibilityId() const;

 private:
  // cppcheck-suppress unusedStructMember
  core::BBox3   local_bbox_;
  // cppcheck-suppress unusedStructMember
  core::BBox3   world_bbox_;
  core::Mat4f   world_matrix_;
  // cppcheck-suppress unusedStructMember
  bool          always_visible_;
  uint64_t      last_frame_         = 0;
  IVisibilitySystem* visibility_system_ = nullptr;
  uintptr_t     visibility_id_      = 0;
};

}  // namespace renderer
