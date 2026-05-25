#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "abstract/VideoDevice.h"
#include "core/BBox3.h"
#include "game/GameLight.h"
#include "game/GameLightDesc.h"
#include "game/GameMaterial.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"
#include "game/MeshTemplate.h"

namespace editor {

// Editor scene: a floor plane, a unit cube, and a global directional light.
//
// All objects are registered with game::GameSystem on construction and
// unregistered on destruction. The Renderer's visibility and light systems
// see them automatically.
//
// Scene contents:
//   - Floor: map_size × map_size horizontal plane with neutral grey material.
//   - Cube:  unit cube placed at (0,1,0), added as initial dynamic object (user-deletable).
//   - Light: global directional light described by the provided GameLightDesc.
//
// Dynamic objects (added via AddDynamicObject) are user-deletable; static objects
// (floor_, light_) are always present and cannot be removed at runtime.
class EditorScene {
 public:
  explicit EditorScene(abstract::VideoDevice* video);
  EditorScene(abstract::VideoDevice* video,
              const std::string& map_name,
              float map_size,
              const game::GameLightDesc& light_desc);
  ~EditorScene();

  EditorScene(const EditorScene&)            = delete;
  EditorScene& operator=(const EditorScene&) = delete;

  // Returns all scene objects (static + dynamic) in creation order.
  [[nodiscard]] const std::vector<game::GameObject*>& GetObjects() const { return objects_; }

  // Registers callbacks fired when dynamic objects are added to or removed from
  // the scene. Covers all paths: direct calls, command execute, undo, and redo.
  void SetOnObjectAdded(std::function<void(game::GameObject*)> cb) {
    on_object_added_ = std::move(cb);
  }
  void SetOnObjectRemoved(std::function<void(game::GameObject*)> cb) {
    on_object_removed_ = std::move(cb);
  }

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

  // Returns the union of all object world bboxes.
  // Guarantees a minimum diagonal of 10 world units for empty scenes.
  [[nodiscard]] core::BBox3                   GetBounds()   const;

  [[nodiscard]] const std::string&           GetMapName()  const;
  void                                        SetMapName(const std::string& name);

  [[nodiscard]] float                         GetMapSize()  const;

  [[nodiscard]] const std::filesystem::path& GetFilePath() const;
  void                                        SetFilePath(const std::filesystem::path& p);

  // cppcheck-suppress returnByReference
  [[nodiscard]] game::GameLightDesc           GetGlobalLightDesc() const;
  // Updates global_light_desc_ and immediately syncs the live renderer light.
  void                                        SetGlobalLightDesc(const game::GameLightDesc& desc);

 private:
  // cppcheck-suppress unusedStructMember
  std::string             map_name_  = "untitled";
  // cppcheck-suppress unusedStructMember
  float                   map_size_  = 120.f;
  // cppcheck-suppress unusedStructMember
  std::filesystem::path   file_path_;
  // cppcheck-suppress unusedStructMember
  game::GameLightDesc     global_light_desc_;

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
  // cppcheck-suppress unusedStructMember
  std::function<void(game::GameObject*)> on_object_added_;
  // cppcheck-suppress unusedStructMember
  std::function<void(game::GameObject*)> on_object_removed_;
};

}  // namespace editor
