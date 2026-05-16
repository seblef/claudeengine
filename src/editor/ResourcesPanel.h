#pragma once

#include <functional>

namespace game { class GameMaterial; }

namespace editor {

// Left panel "Resources" tab: tree view of loaded materials and mesh templates.
//
// Materials come from game::GameMaterial::GetRegistry() (procedural "__proc_"
// entries are excluded). Mesh templates come from MeshTemplate::GetAll().
// Double-clicking a material leaf invokes the on_material_open_ callback.
class ResourcesPanel {
 public:
  ResourcesPanel() = default;

  // Sets the callback invoked when the user double-clicks a material leaf.
  void SetOnMaterialOpen(std::function<void(game::GameMaterial*)> cb) {
    on_material_open_ = std::move(cb);
  }

  // Renders the resources tree inside the current ImGui tab item.
  void Render();

 private:
  std::function<void(game::GameMaterial*)> on_material_open_;
};

}  // namespace editor
