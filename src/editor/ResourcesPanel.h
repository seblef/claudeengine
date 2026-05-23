#pragma once

#include <functional>
#include <string_view>

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

  // Sets the callback invoked when the user clicks the "New Material" (+) button
  // and confirms the name. Receives the chosen name.
  void SetOnNewMaterial(std::function<void(std::string_view)> cb) {
    on_new_material_ = std::move(cb);
  }

  // Sets the callback invoked when the user clicks the "Import Material" button.
  void SetOnImportMaterial(std::function<void()> cb) {
    on_import_material_ = std::move(cb);
  }

  // Renders the resources tree inside the current ImGui tab item.
  void Render();

 private:
  std::function<void(game::GameMaterial*)> on_material_open_;
  std::function<void(std::string_view)>    on_new_material_;
  std::function<void()>                    on_import_material_;

  // cppcheck-suppress unusedStructMember
  bool show_new_mat_modal_    = false;
  // cppcheck-suppress unusedStructMember
  char new_mat_name_buf_[128] = {};
};

}  // namespace editor
