#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace game {
class GameMaterial;
class MeshTemplate;
}

namespace editor {

// Left panel "Resources" tab: tree view of loaded materials, mesh templates,
// particle effects, and sound templates.
//
// Materials come from game::GameMaterial::GetRegistry() (procedural "__proc_"
// entries are excluded). Mesh templates come from MeshTemplate::GetAll().
// Sound templates are discovered by scanning data/sounds/*.sound.yaml on disk.
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

  // Sets the callback invoked when the user clicks the "Import Mesh" button.
  void SetOnImportMesh(std::function<void()> cb) {
    on_import_mesh_ = std::move(cb);
  }

  // Sets the callback invoked when the user right-clicks a mesh and selects "Edit…".
  // Receives the MeshTemplate to edit (const; caller uses its id/path for OpenEdit).
  void SetOnMeshEdit(std::function<void(const game::MeshTemplate*)> cb) {
    on_mesh_edit_ = std::move(cb);
  }

  // Sets the callback invoked when the user double-clicks a particle effect.
  // Receives the template name (basename without extension).
  void SetOnParticleOpen(std::function<void(const std::string&)> cb) {
    on_particle_open_ = std::move(cb);
  }

  // Sets the callback invoked when the user double-clicks a sound template leaf.
  // Receives the template name (basename without .sound.yaml extension).
  void SetOnSoundOpen(std::function<void(const std::string&)> cb) {
    on_sound_open_ = std::move(cb);
  }

  // Sets the callback invoked when the user confirms the "New Sound Template"
  // dialog. Receives the chosen name (without extension).
  void SetOnNewSound(std::function<void(std::string_view)> cb) {
    on_new_sound_ = std::move(cb);
  }

  // Renders the resources tree inside the current ImGui tab item.
  void Render();

 private:
  std::function<void(game::GameMaterial*)> on_material_open_;
  std::function<void(std::string_view)>    on_new_material_;
  std::function<void()>                    on_import_material_;
  std::function<void()>                    on_import_mesh_;
  std::function<void(const game::MeshTemplate*)> on_mesh_edit_;
  // cppcheck-suppress unusedStructMember
  std::function<void(const std::string&)>  on_particle_open_;
  // cppcheck-suppress unusedStructMember
  std::function<void(const std::string&)>  on_sound_open_;
  // cppcheck-suppress unusedStructMember
  std::function<void(std::string_view)>    on_new_sound_;

  // cppcheck-suppress unusedStructMember
  bool show_new_mat_modal_    = false;
  // cppcheck-suppress unusedStructMember
  char new_mat_name_buf_[128] = {};

  // cppcheck-suppress unusedStructMember
  bool show_new_sound_modal_    = false;
  // cppcheck-suppress unusedStructMember
  char new_sound_name_buf_[128] = {};
};

}  // namespace editor
