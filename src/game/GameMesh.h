#pragma once

#include <memory>

#include "game/GameObject.h"
#include "game/MeshTemplate.h"
#include "renderer/RenderableMeshInstance.h"

namespace game {

// A game object that renders a mesh from a shared MeshTemplate.
//
// The underlying RenderableMeshInstance is created when the object is added to
// the scene and destroyed when it is removed, so GPU resources are held only
// while the object participates in rendering.
//
// Multiple GameMesh objects may share the same MeshTemplate; the geometry and
// materials are loaded from disk only once.
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

  // Creates a RenderableMeshInstance and registers it with the Renderer.
  void OnAddedToScene() override;

  // Unregisters the instance from the Renderer and destroys it.
  void OnRemovedFromScene() override;

  // Forwards the new world transform to the RenderableMeshInstance if present.
  void OnWorldTransformUpdated() override;

 private:
  // cppcheck-suppress unusedStructMember
  MeshTemplate* template_;
  // cppcheck-suppress unusedStructMember
  bool          always_visible_;
  std::unique_ptr<renderer::RenderableMeshInstance> instance_;
};

}  // namespace game
