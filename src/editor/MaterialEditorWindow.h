#pragma once

namespace game { class GameMaterial; }

namespace editor {

// Floating window for viewing and editing a game material.
//
// Open() sets the material to display and makes the window visible.
// Render() must be called each frame between ImGui::NewFrame() and
// ImGui::Render(). Full editing UI is implemented in Issue #208.
class MaterialEditorWindow {
 public:
  MaterialEditorWindow() = default;

  // Opens the window for the given material. Passing nullptr hides the window.
  void Open(game::GameMaterial* mat);

  // Renders the window if a material is currently open.
  void Render();

 private:
  // cppcheck-suppress unusedStructMember
  game::GameMaterial* material_ = nullptr;
  // cppcheck-suppress unusedStructMember
  bool open_ = false;
};

}  // namespace editor
