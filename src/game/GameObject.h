#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/GameObjectType.h"
#include "game/GameObjectVisitor.h"

namespace game {

// Abstract base for all game objects (meshes, lights, cameras).
//
// Manages a local transform relative to an optional parent and a derived world
// transform. Supports a parent-child hierarchy: SetLocalTransform() is the
// primary mutator and propagates the world transform recursively to all
// descendants. SetWorldTransform() derives the local transform when a parent
// is present, then delegates to SetLocalTransform().
//
// Do not modify world_transform_ or local_transform_ directly.
class GameObject {
 public:
  // local_bbox: AABB in local (object) space; fixed for the object's lifetime.
  GameObject(GameObjectType type, const core::BBox3& local_bbox);

  virtual ~GameObject() = default;

  GameObject(const GameObject&)            = delete;
  GameObject& operator=(const GameObject&) = delete;
  GameObject(GameObject&&)                 = delete;
  GameObject& operator=(GameObject&&)      = delete;

  // Sets the local transform, recomputes world_transform_ =
  // parent->world_transform_ * local (identity parent when no parent), updates
  // world_bbox_, calls OnWorldTransformUpdated(), then recursively propagates
  // to all descendants.
  void SetLocalTransform(const core::Mat4f& local);

  // Derives local_transform_ from the parent world transform and calls
  // SetLocalTransform(). When no parent, equivalent to SetLocalTransform().
  void SetWorldTransform(const core::Mat4f& transform);

  // Links child to this object as a child. Recomputes child->local_transform_
  // from the current world transforms so the child's world position is
  // preserved. child must not already have a parent.
  void AddChild(GameObject* child);

  // Unlinks child from this object. The child retains its current world
  // transform; its local_transform_ becomes equal to its world_transform_.
  void RemoveChild(GameObject* child);

  // Moves this object to new_parent: removes from current parent (if any)
  // then calls new_parent->AddChild(). Passing nullptr detaches from the
  // current parent without attaching to a new one.
  void Reparent(GameObject* new_parent);

  void SetName(const std::string& name);
  [[nodiscard]] const std::string& GetName() const;

  [[nodiscard]] const core::Mat4f&          GetLocalTransform() const;
  [[nodiscard]] const core::Mat4f&          GetWorldTransform() const;
  [[nodiscard]] const core::BBox3&          GetLocalBBox()      const;
  [[nodiscard]] const core::BBox3&          GetWorldBBox()      const;
  [[nodiscard]] GameObjectType              GetType()            const;
  [[nodiscard]] GameObject*                 GetParent()          const;
  [[nodiscard]] const std::vector<GameObject*>& GetChildren()   const;

  // Called once per frame by GameSystem for root objects; propagates to children.
  // Default implementation propagates to children only — subclasses override to
  // add per-frame logic before the child propagation.
  // Subclasses must call GameObject::Update(dt) at the end to propagate.
  virtual void Update(float dt);

  // Dispatches to the matching visitor overload for this object's concrete type.
  virtual void Accept(GameObjectVisitor& visitor) = 0;

  // Returns a clone placed at position, preserving orientation and scale from
  // the original world transform. Returns nullptr for non-copyable types
  // (GameTerrain, GamePlayerStart, GameCamera).
  virtual std::unique_ptr<GameObject> Copy(const core::Vec3f& position) const;

  // Called after the world transform and world bbox have been updated.
  virtual void OnWorldTransformUpdated() = 0;

  // Called by GameSystem when this object is added to the scene.
  virtual void OnAddedToScene() = 0;

  // Called by GameSystem when this object is removed from the scene.
  virtual void OnRemovedFromScene() = 0;

 protected:
  // Physics-driven world transform update: updates world_transform_ and
  // local_transform_ from the simulation result, then propagates downward to
  // children. Must NOT back-propagate to parent_ to avoid loops.
  // Called only from IPhysicsBodyListener::OnBodyTransformUpdated().
  void SetWorldTransformPhysics(const core::Mat4f& transform);

 private:
  // Recomputes world_transform_ from parent and local_transform_, updates
  // world_bbox_, calls OnWorldTransformUpdated(), then recurses into children.
  void UpdateWorldTransform();

  // cppcheck-suppress unusedStructMember
  GameObjectType type_;
  // cppcheck-suppress unusedStructMember
  std::string    name_ = "Object";
  // cppcheck-suppress unusedStructMember
  core::BBox3    local_bbox_;
  // cppcheck-suppress unusedStructMember
  core::BBox3    world_bbox_;
  // cppcheck-suppress unusedStructMember
  core::Mat4f    local_transform_;
  core::Mat4f    world_transform_;
  // cppcheck-suppress unusedStructMember
  GameObject*    parent_ = nullptr;
  // cppcheck-suppress unusedStructMember
  std::vector<GameObject*> children_;
};

}  // namespace game
