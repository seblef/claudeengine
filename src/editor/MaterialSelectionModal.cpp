#include "editor/MaterialSelectionModal.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "core/Config.h"

namespace editor {

MaterialSelectionModal::MaterialSelectionModal(const char* title)
    : title_(title) {}

void MaterialSelectionModal::Open() {
  entries_.clear();
  selected_ = -1;

  const auto materials_dir = core::Config::GetDataFolder() / "materials";
  if (std::filesystem::exists(materials_dir)) {
    for (const auto& entry : std::filesystem::directory_iterator(materials_dir)) {
      const auto& p = entry.path();
      if (entry.is_regular_file() && p.extension() == ".yaml")
        entries_.push_back(p.stem().string());
    }
    std::sort(entries_.begin(), entries_.end());
  }

  is_open_ = true;
}

std::string MaterialSelectionModal::Render() {
  if (is_open_) {
    ImGui::OpenPopup(title_.c_str());
    is_open_ = false;
  }

  if (!ImGui::BeginPopupModal(title_.c_str(), nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return {};
  }

  std::string result;

  if (entries_.empty()) {
    ImGui::TextUnformatted("No .yaml files found in data/materials/");
  } else {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
      if (ImGui::Selectable(entries_[i].c_str(), selected_ == i,
                            ImGuiSelectableFlags_DontClosePopups))
        selected_ = i;
    }
  }

  ImGui::Separator();

  const bool can_ok = (selected_ >= 0 &&
                       selected_ < static_cast<int>(entries_.size()));
  if (!can_ok) ImGui::BeginDisabled();
  if (ImGui::Button("OK")) {
    result    = entries_[selected_];
    selected_ = -1;
    ImGui::CloseCurrentPopup();
  }
  if (!can_ok) ImGui::EndDisabled();

  ImGui::SameLine();

  if (ImGui::Button("Cancel")) {
    selected_ = -1;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
  return result;
}

}  // namespace editor
