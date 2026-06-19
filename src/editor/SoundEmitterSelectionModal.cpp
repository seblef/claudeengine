#include "editor/SoundEmitterSelectionModal.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "core/Config.h"

namespace editor {

void SoundEmitterSelectionModal::Open() {
  entries_.clear();
  selected_ = -1;

  const auto sounds_dir = core::Config::GetDataFolder() / "sounds";
  if (std::filesystem::exists(sounds_dir)) {
    for (const auto& entry : std::filesystem::directory_iterator(sounds_dir)) {
      const auto& p = entry.path();
      // Match *.sound.yaml: extension=".yaml", stem extension=".sound"
      if (p.extension() == ".yaml" && p.stem().extension() == ".sound") {
        entries_.push_back(p.stem().stem().string());
      }
    }
    std::sort(entries_.begin(), entries_.end());
  }

  is_open_ = true;
}

std::string SoundEmitterSelectionModal::Render() {
  if (is_open_) {
    ImGui::OpenPopup("Select Sound Template");
    is_open_ = false;
  }

  if (!ImGui::BeginPopupModal("Select Sound Template", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return {};
  }

  std::string result;

  if (entries_.empty()) {
    ImGui::TextUnformatted("No .sound.yaml files found in data/sounds/");
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
