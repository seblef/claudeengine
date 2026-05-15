#pragma once

#include <memory>
#include <vector>

#include "abstract/VideoDevice.h"
#include "game/GameLight.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"

namespace editor {

// Default editor scene: a floor plane, a unit cube, and a global directional light.
//
// All objects are registered with game::GameSystem on construction and
// unregistered on destruction. The Renderer's visibility and light systems
// see them automatically.
//
// Scene contents:
//   - Floor: 120×120 (half_size=60) horizontal plane with neutral grey material.
//   - Cube:  unit cube scaled ×2, centred at (0,1,0) — bottom face sits on the floor.
//   - Light: global directional light (sun-like), colour (0.9, 0.85, 0.7), intensity 1.2.
class EditorScene {
 public:
  explicit EditorScene(abstract::VideoDevice* video);
  ~EditorScene();

  EditorScene(const EditorScene&)            = delete;
  EditorScene& operator=(const EditorScene&) = delete;

  // Returns all scene objects (floor, cube, light) in creation order.
  [[nodiscard]] const std::vector<game::GameObject*>& GetObjects() const { return objects_; }

  [[nodiscard]] game::GameObject* GetSelectedObject() const { return selected_; }
  void SetSelectedObject(game::GameObject* obj)             { selected_ = obj; }

 private:
  std::unique_ptr<game::GameMesh>  floor_;
  std::unique_ptr<game::GameMesh>  cube_;
  std::unique_ptr<game::GameLight> light_;

  std::vector<game::GameObject*> objects_;
  // cppcheck-suppress unusedStructMember
  game::GameObject*              selected_ = nullptr;
};

}  // namespace editor
