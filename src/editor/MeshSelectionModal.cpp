#include "editor/MeshSelectionModal.h"

#include <imgui.h>

namespace editor {

void MeshSelectionModal::Open() {
  is_open_  = true;
  selected_ = nullptr;
}

game::MeshTemplate* MeshSelectionModal::Render() {
  if (is_open_) {
    ImGui::OpenPopup("Select Mesh Template");
    is_open_ = false;
  }

  game::MeshTemplate* result = nullptr;

  if (!ImGui::BeginPopupModal("Select Mesh Template", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return nullptr;
  }

  for (auto& [name, tmpl] : game::MeshTemplate::GetAll()) {
    if (ImGui::Selectable(name.c_str(), selected_ == tmpl))
      selected_ = tmpl;
  }

  ImGui::Separator();

  const bool can_ok = (selected_ != nullptr);
  if (!can_ok) ImGui::BeginDisabled();
  if (ImGui::Button("OK")) {
    result    = selected_;
    selected_ = nullptr;
    ImGui::CloseCurrentPopup();
  }
  if (!can_ok) ImGui::EndDisabled();

  ImGui::SameLine();

  if (ImGui::Button("Cancel")) {
    selected_ = nullptr;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
  return result;
}

}  // namespace editor
