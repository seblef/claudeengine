#pragma once

#include <memory>
#include <optional>

#include "game/GameObject.h"
#include "game/MeshTemplate.h"
#include "physics/IPhysicsBodyListener.h"
#include "physics/PhysicsBodyDesc.h"
#include "renderer/MeshInstance.h"

namespace physics {
class PhysicsBody;
}  // namespace physics

namespace game {

// A game object that renders a mesh from a shared MeshTemplate.
//
// The underlying MeshInstance is created in the constructor and destroyed in
// the destructor. OnAddedToScene / OnRemovedFromScene register and unregister
// it with the Renderer without affecting its lifetime.
//
// Multiple GameMesh objects may share the same MeshTemplate; the geometry and
// material are loaded from disk only once.
//
// An optional physics body can be attached via SetPhysicsDesc(). Primitive
// shapes (Box, Sphere, Cylinder, Capsule) are created with CreateBody();
// ConvexHull and Exact shapes are built from the template's CPU-side geometry.
// For Dynamic bodies, OnBodyTransformUpdated() is called each Step() and
// propagates the simulation result back to the game-side transform.
class GameMesh : public GameObject, public physics::IPhysicsBodyListener {
 public:
  // tmpl must not be null. Calls tmpl->AddRef().
  explicit GameMesh(MeshTemplate* tmpl, bool always_visible = false);

  // Calls template_->Release() and destroys any live physics body.
  ~GameMesh() override;

  GameMesh(const GameMesh&)            = delete;
  GameMesh& operator=(const GameMesh&) = delete;
  GameMesh(GameMesh&&)                 = delete;
  GameMesh& operator=(GameMesh&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override { visitor.Visit(*this); }

  // Returns a new GameMesh from the same template, placed at position.
  [[nodiscard]] std::unique_ptr<game::GameObject> Copy(
      const core::Vec3f& position) const override;

  // Registers the instance with the Renderer and creates any pending physics body.
  void OnAddedToScene() override;

  // Destroys the physics body (if any) and unregisters the instance from the Renderer.
  void OnRemovedFromScene() override;

  // Forwards the new world transform to the MeshInstance and, for Static or
  // Kinematic bodies, pushes the transform into the physics simulation.
  void OnWorldTransformUpdated() override;

  // Attaches a physics body description to this mesh.
  // If the mesh is already in scene, the existing body is replaced immediately.
  void SetPhysicsDesc(const physics::PhysicsBodyDesc& desc);

  // Removes the physics body description from this mesh.
  // If the mesh is already in scene, the live physics body is destroyed.
  void ClearPhysicsDesc();

  // IPhysicsBodyListener — called by PhysicsSystem::Step() for Dynamic bodies.
  // Propagates the simulation-driven transform to the game-side world transform.
  void OnBodyTransformUpdated(const core::Mat4f& transform) override;

  // Returns the shared template (e.g. so a material editor can call SetMaterial()).
  [[nodiscard]] MeshTemplate* GetTemplate() const;

  // Returns the physics body description, or nullopt if none has been set.
  [[nodiscard]] const std::optional<physics::PhysicsBodyDesc>& GetPhysicsDesc() const {
    return physics_desc_;
  }

  // Returns the live physics body, or nullptr if none exists (not yet in scene,
  // or no physics desc set).
  [[nodiscard]] const physics::PhysicsBody* GetPhysicsBody() const {
    return physics_body_;
  }

  // Controls rendering visibility. When set to false the mesh is removed from
  // the renderer's renderable list; when set to true it is re-added.
  // Has no effect while the mesh is not in scene.
  void SetVisible(bool visible);

  [[nodiscard]] bool IsVisible() const { return visible_; }

 private:
  void CreatePhysicsBody();
  void DestroyPhysicsBody();

  // cppcheck-suppress unusedStructMember
  MeshTemplate* template_;
  // cppcheck-suppress unusedStructMember
  bool          always_visible_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::MeshInstance> instance_;
  // cppcheck-suppress unusedStructMember
  std::optional<physics::PhysicsBodyDesc> physics_desc_;
  // Non-owning; lifetime is managed by PhysicsSystem.
  // cppcheck-suppress unusedStructMember
  physics::PhysicsBody* physics_body_ = nullptr;
  // cppcheck-suppress unusedStructMember
  bool in_scene_ = false;
  // cppcheck-suppress unusedStructMember
  bool visible_  = true;
};

}  // namespace game
