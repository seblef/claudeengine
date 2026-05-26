#include "editor/TerrainCreationDialog.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

namespace editor {

void TerrainCreationDialog::Open() {
  is_open_ = true;
}

bool TerrainCreationDialog::Render() {
  if (is_open_) {
    ImGui::OpenPopup("New Terrain##modal");
    is_open_ = false;
  }

  if (!ImGui::BeginPopupModal("New Terrain##modal", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return false;
  }

  ImGui::DragFloat("Size X (m)",        &params_.size_x,     10.f, 10.f,
                   100000.f, "%.0f");
  ImGui::DragFloat("Size Z (m)",        &params_.size_z,     10.f, 10.f,
                   100000.f, "%.0f");
  ImGui::DragFloat("Resolution (m/texel)", &params_.resolution, 0.05f, 0.1f,
                   100.f, "%.2f");
  ImGui::DragFloat("Min height (m)",    &params_.min_height, 1.f, -10000.f,
                   10000.f, "%.1f");
  ImGui::DragFloat("Max height (m)",    &params_.max_height, 1.f, -10000.f,
                   10000.f, "%.1f");

  // Derived info: texel dimensions and memory footprint.
  const float safe_res = std::max(0.001f, params_.resolution);
  const int   w_texels = std::max(2, static_cast<int>(
                             std::round(params_.size_x / safe_res)));
  const int   h_texels = std::max(2, static_cast<int>(
                             std::round(params_.size_z / safe_res)));
  const float size_mb  = static_cast<float>(w_texels) *
                         static_cast<float>(h_texels) * 2.f / (1024.f * 1024.f);

  ImGui::Spacing();
  ImGui::Text("Heightmap: %d \xc3\x97 %d texels \xe2\x80\x94 %.1f MB",
              w_texels, h_texels, size_mb);

  // Warnings.
  constexpr float kMaxMB  = 256.f;
  constexpr float kMinRes = 0.5f;
  if (size_mb > kMaxMB) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.6f, 0.f, 1.f));
    ImGui::TextUnformatted("Warning: heightmap exceeds 256 MB");
    ImGui::PopStyleColor();
  }
  if (params_.resolution < kMinRes) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.6f, 0.f, 1.f));
    ImGui::TextUnformatted("Warning: resolution below 0.5 m/texel");
    ImGui::PopStyleColor();
  }

  ImGui::Separator();

  bool confirmed = false;
  if (ImGui::Button("Create")) {
    confirmed = true;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel"))
    ImGui::CloseCurrentPopup();

  ImGui::EndPopup();
  return confirmed;
}

}  // namespace editor
