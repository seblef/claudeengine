#include "editor/ParticleSystemSelectionModal.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <imgui.h>

#include "core/Config.h"

namespace editor {

void ParticleSystemSelectionModal::Open() {
  entries_.clear();
  selected_ = -1;

  const auto particles_dir = core::Config::GetDataFolder() / "particles";
  if (std::filesystem::exists(particles_dir)) {
    for (const auto& entry : std::filesystem::directory_iterator(particles_dir)) {
      const auto& p = entry.path();
      if (p.extension() == ".yaml" && p.stem().extension() == ".particles") {
        // e.g. smoke.particles.yaml → stem="smoke.particles" → stem again="smoke"
        entries_.push_back(p.stem().stem().string());
      }
    }
    std::sort(entries_.begin(), entries_.end());
  }

  is_open_ = true;
}

std::string ParticleSystemSelectionModal::Render() {
  if (is_open_) {
    ImGui::OpenPopup("Select Particle Effect");
    is_open_ = false;
  }

  if (!ImGui::BeginPopupModal("Select Particle Effect", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return {};
  }

  std::string result;

  if (entries_.empty()) {
    ImGui::TextUnformatted("No .particles.yaml files found in data/particles/");
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
    result   = entries_[selected_];
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
