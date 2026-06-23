#include "editor/VehicleSelectionModal.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "core/Config.h"

namespace editor {

void VehicleSelectionModal::Open() {
  entries_.clear();
  selected_ = -1;

  const auto vehicles_dir = core::Config::GetDataFolder() / "vehicles";
  if (std::filesystem::exists(vehicles_dir)) {
    for (const auto& entry : std::filesystem::directory_iterator(vehicles_dir)) {
      const auto& p = entry.path();
      if (p.extension() == ".yaml" && p.stem().extension() == ".vehicle") {
        // e.g. duke.vehicle.yaml → stem="duke.vehicle" → stem again="duke"
        entries_.push_back(p.stem().stem().string());
      }
    }
    std::sort(entries_.begin(), entries_.end());
  }

  is_open_ = true;
}

std::string VehicleSelectionModal::Render() {
  if (is_open_) {
    ImGui::OpenPopup("Select Vehicle");
    is_open_ = false;
  }

  if (!ImGui::BeginPopupModal("Select Vehicle", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return {};
  }

  std::string result;

  if (entries_.empty()) {
    ImGui::TextUnformatted("No .vehicle.yaml files found in data/vehicles/");
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
