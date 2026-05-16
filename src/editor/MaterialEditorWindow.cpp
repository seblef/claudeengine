#include "editor/MaterialEditorWindow.h"

#include <imgui.h>

#include "game/GameMaterial.h"

namespace editor {

void MaterialEditorWindow::Open(game::GameMaterial* mat) {
  material_ = mat;
  open_     = (mat != nullptr);
}

void MaterialEditorWindow::Render() {
  if (!open_ || !material_) return;

  const std::string title = "Material: " + material_->GetId();
  if (!ImGui::Begin(title.c_str(), &open_)) {
    ImGui::End();
    return;
  }

  ImGui::TextUnformatted("Material editing — implemented in Issue #208.");

  ImGui::End();
}

}  // namespace editor
