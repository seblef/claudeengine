#pragma once

#include <memory>

#include "game/GameObject.h"
#include "game/MeshTemplate.h"
#include "renderer/MeshInstance.h"

namespace game {

// A game object that renders a mesh from a shared MeshTemplate.
//
// The underlying MeshInstance is created in the constructor and destroyed in
// the destructor. OnAddedToScene / OnRemovedFromScene register and unregister
// it with the Renderer without affecting its lifetime.
//
// Multiple GameMesh objects may share the same MeshTemplate; the geometry and
// material are loaded from disk only once.
class GameMesh : public GameObject {
 public:
  // tmpl must not be null. Calls tmpl->AddRef().
  explicit GameMesh(MeshTemplate* tmpl, bool always_visible = false);

  // Calls template_->Release().
  ~GameMesh() override;

  GameMesh(const GameMesh&)            = delete;
  GameMesh& operator=(const GameMesh&) = delete;
  GameMesh(GameMesh&&)                 = delete;
  GameMesh& operator=(GameMesh&&)      = delete;

  void Accept(GameObjectVisitor& visitor) override { visitor.Visit(*this); }

  // Registers the instance with the Renderer.
  void OnAddedToScene() override;

  // Unregisters the instance from the Renderer.
  void OnRemovedFromScene() override;

  // Forwards the new world transform to the MeshInstance.
  void OnWorldTransformUpdated() override;

  // Returns the shared template (e.g. so a material editor can call SetMaterial()).
  [[nodiscard]] MeshTemplate* GetTemplate() const;

 private:
  // cppcheck-suppress unusedStructMember
  MeshTemplate* template_;
  // cppcheck-suppress unusedStructMember
  bool          always_visible_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<renderer::MeshInstance> instance_;
};

}  // namespace game
