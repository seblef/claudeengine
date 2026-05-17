#pragma once

#include <memory>
#include <vector>

#include "abstract/VideoDevice.h"
#include "game/GameLight.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"
#include "game/MeshTemplate.h"

namespace editor {

// Default editor scene: a floor plane, a unit cube, and a global directional light.
//
// All objects are registered with game::GameSystem on construction and
// unregistered on destruction. The Renderer's visibility and light systems
// see them automatically.
//
// Scene contents:
//   - Floor: 120×120 (half_size=60) horizontal plane with neutral grey material.
//   - Cube:  unit cube placed at (0,1,0), added as initial dynamic object (user-deletable).
//   - Light: global directional light (sun-like), colour (0.9, 0.85, 0.7), intensity 1.2.
//
// Dynamic objects (added via AddDynamicObject) are user-deletable; static objects
// (floor_, light_) are always present and cannot be removed at runtime.
class EditorScene {
 public:
  explicit EditorScene(abstract::VideoDevice* video);
  ~EditorScene();

  EditorScene(const EditorScene&)            = delete;
  EditorScene& operator=(const EditorScene&) = delete;

  // Returns all scene objects (static + dynamic) in creation order.
  [[nodiscard]] const std::vector<game::GameObject*>& GetObjects() const { return objects_; }

  // Transfers ownership of obj into the dynamic pool, registers it with
  // GameSystem, appends it to GetObjects(), and returns the raw pointer.
  game::GameObject* AddDynamicObject(std::unique_ptr<game::GameObject> obj);

  // Removes obj from the dynamic pool and unregisters it from GameSystem.
  // Clears the selection if obj was selected.
  // No-op if obj is not a dynamic object (e.g. floor or global light).
  void RemoveDynamicObject(game::GameObject* obj);

  // Removes obj from the dynamic pool, unregisters it from GameSystem, and
  // returns ownership to the caller. Returns nullptr if obj is not dynamic.
  // Clears the selection if obj was selected.
  [[nodiscard]] std::unique_ptr<game::GameObject> ReclaimDynamicObject(
      game::GameObject* obj);

  // Returns true if obj was added via AddDynamicObject (i.e. user-deletable).
  [[nodiscard]] bool IsDynamic(const game::GameObject* obj) const;

  // Registers an imported game material. Stores it for lifetime management;
  // the material is released on EditorScene destruction.
  void AddGameMaterial(game::GameMaterial* mat) {
    game_materials_.push_back(mat);
  }

  // Registers an imported mesh template. Stores it for lifetime management;
  // the template is released on EditorScene destruction.
  void AddMeshTemplate(game::MeshTemplate* tmpl) {
    mesh_templates_.push_back(tmpl);
  }

  [[nodiscard]] game::GameObject* GetSelectedObject() const { return selected_; }
  void SetSelectedObject(game::GameObject* obj)             { selected_ = obj; }

 private:
  // Static objects — always present, never deletable by the user.
  // dynamic_objects_ is declared first so it is destroyed before these,
  // preventing dangling references during shutdown.
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<game::GameObject>> dynamic_objects_;

  std::unique_ptr<game::GameMesh>  floor_;
  std::unique_ptr<game::GameLight> light_;

  // Flat view of all objects (static raw ptrs + dynamic raw ptrs), kept in sync.
  std::vector<game::GameObject*>    objects_;
  std::vector<game::GameMaterial*>  game_materials_;
  std::vector<game::MeshTemplate*>  mesh_templates_;
  // cppcheck-suppress unusedStructMember
  game::GameObject*                selected_ = nullptr;
};

}  // namespace editor
