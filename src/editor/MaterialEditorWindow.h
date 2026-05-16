#pragma once

#include <memory>

#include "abstract/VideoDevice.h"
#include "editor/MeshPreview.h"

namespace game { class GameMaterial; }

namespace editor {

// Floating window for viewing and editing a game material.
//
// Open() sets the material to display and makes the window visible.
// Render() must be called each frame between ImGui::NewFrame() and
// ImGui::Render(). Full editing UI (texture/color slots, save, apply-to-
// selection) is implemented in Issue #208.
class MaterialEditorWindow {
 public:
  explicit MaterialEditorWindow(abstract::VideoDevice* video);
  ~MaterialEditorWindow();

  // Opens the window for the given material. Passing nullptr hides the window.
  void Open(game::GameMaterial* mat);

  // Renders the window if a material is currently open.
  void Render();

 private:
  // cppcheck-suppress unusedStructMember
  game::GameMaterial* material_ = nullptr;
  // cppcheck-suppress unusedStructMember
  bool         open_    = false;
  MeshPreview  preview_;
};

}  // namespace editor
