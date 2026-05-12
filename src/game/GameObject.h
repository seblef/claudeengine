#pragma once

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "game/GameObjectType.h"

namespace game {

// Abstract base for all game objects (meshes, lights, cameras).
//
// Manages world transform and the derived world bounding box. Subclasses
// react to spatial and lifecycle changes by overriding the three pure virtual
// event methods.
//
// SetWorldTransform() is the single entry point for spatial updates: it
// stores the matrix, recomputes the world bbox from the local bbox, and
// calls OnWorldTransformUpdated(). Do not modify world_transform_ directly.
class GameObject {
 public:
  // local_bbox: AABB in local (object) space; fixed for the object's lifetime.
  GameObject(GameObjectType type, const core::BBox3& local_bbox);

  virtual ~GameObject() = default;

  GameObject(const GameObject&)            = delete;
  GameObject& operator=(const GameObject&) = delete;
  GameObject(GameObject&&)                 = delete;
  GameObject& operator=(GameObject&&)      = delete;

  // Sets the world transform, recomputes the world bbox as
  // local_bbox * transform, and calls OnWorldTransformUpdated().
  void SetWorldTransform(const core::Mat4f& transform);

  [[nodiscard]] const core::Mat4f& GetWorldTransform() const;
  [[nodiscard]] const core::BBox3& GetLocalBBox()      const;
  [[nodiscard]] const core::BBox3& GetWorldBBox()      const;
  [[nodiscard]] GameObjectType     GetType()            const;

  // Called after the world transform and world bbox have been updated.
  virtual void OnWorldTransformUpdated() = 0;

  // Called by GameSystem when this object is added to the scene.
  virtual void OnAddedToScene() = 0;

  // Called by GameSystem when this object is removed from the scene.
  virtual void OnRemovedFromScene() = 0;

 private:
  // cppcheck-suppress unusedStructMember
  GameObjectType type_;
  // cppcheck-suppress unusedStructMember
  core::BBox3    local_bbox_;
  // cppcheck-suppress unusedStructMember
  core::BBox3    world_bbox_;
  core::Mat4f    world_transform_;
};

}  // namespace game
