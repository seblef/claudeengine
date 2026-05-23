#include "editor/MapPropertiesWindow.h"

#include <cstring>
#include <string>

#include <imgui.h>

#include "core/Color.h"
#include "core/Vec3f.h"
#include "editor/EditorScene.h"
#include "game/GameLightDesc.h"

namespace editor {

MapPropertiesWindow::MapPropertiesWindow(EditorScene* scene)
    : scene_(scene) {}

// --- Panel mode -------------------------------------------------------------

bool MapPropertiesWindow::RenderPanel() {
  bool changed = false;
  game::GameLightDesc desc = scene_->GetGlobalLightDesc();

  // Map name — applied on each keystroke.
  char name_buf[256];
  const std::string& cur_name = scene_->GetMapName();
  std::strncpy(name_buf, cur_name.c_str(), sizeof(name_buf) - 1);
  name_buf[sizeof(name_buf) - 1] = '\0';
  if (ImGui::InputText("Map name", name_buf, sizeof(name_buf))) {
    scene_->SetMapName(std::string(name_buf));
    changed = true;
  }

  // Filename — derived, read-only.
  const std::string filename = cur_name + ".map.yaml";
  ImGui::LabelText("Filename", "%s", filename.c_str());

  // Map size — always read-only in panel mode.
  float size = scene_->GetMapSize();
  ImGui::BeginDisabled();
  ImGui::InputFloat("Map size", &size, 0.f, 0.f, "%.1f",
                    ImGuiInputTextFlags_ReadOnly);
  ImGui::EndDisabled();

  if (RenderLightFields(desc, /*read_only_size=*/true)) {
    scene_->SetGlobalLightDesc(desc);
    changed = true;
  }

  return changed;
}

// --- Modal mode -------------------------------------------------------------

bool MapPropertiesWindow::RenderModal() {
  bool confirmed = false;

  if (!ImGui::BeginPopupModal("New Map##modal", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    return false;
  }

  // Map name.
  char name_buf[256];
  std::strncpy(name_buf, modal_name_.c_str(), sizeof(name_buf) - 1);
  name_buf[sizeof(name_buf) - 1] = '\0';
  if (ImGui::InputText("Map name", name_buf, sizeof(name_buf)))
    modal_name_ = std::string(name_buf);

  // Filename — derived, read-only.
  const std::string filename = modal_name_ + ".map.yaml";
  ImGui::LabelText("Filename", "%s", filename.c_str());

  // Map size — editable in modal mode.
  ImGui::InputFloat("Map size", &modal_size_, 0.f, 0.f, "%.1f");

  RenderLightFields(modal_light_, /*read_only_size=*/false);

  ImGui::Separator();

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

// --- Accessors --------------------------------------------------------------

const std::string& MapPropertiesWindow::GetNewMapName() const {
  return modal_name_;
}

float MapPropertiesWindow::GetNewMapSize() const {
  return modal_size_;
}

const game::GameLightDesc& MapPropertiesWindow::GetNewMapLightDesc() const {
  return modal_light_;
}

// --- Private helpers --------------------------------------------------------

bool MapPropertiesWindow::RenderLightFields(game::GameLightDesc& desc,
                                            bool read_only_size) {
  (void)read_only_size;  // reserved for future use; currently unused
  bool changed = false;

  ImGui::SeparatorText("Global light");

  // Direction — normalized on change.
  float dir[3] = {desc.direction.x, desc.direction.y, desc.direction.z};
  if (ImGui::DragFloat3("Direction", dir, 0.01f)) {
    const core::Vec3f v(dir[0], dir[1], dir[2]);
    desc.direction = v.Normalized();
    changed = true;
  }

  // Color.
  float col[3] = {desc.color.r, desc.color.g, desc.color.b};
  if (ImGui::ColorEdit3("Color", col)) {
    desc.color = core::Color(col[0], col[1], col[2]);
    changed = true;
  }

  // Ambient color.
  float amb[3] = {desc.ambient_color.x, desc.ambient_color.y,
                  desc.ambient_color.z};
  if (ImGui::ColorEdit3("Ambient", amb)) {
    desc.ambient_color = core::Vec3f(amb[0], amb[1], amb[2]);
    changed = true;
  }

  // Intensity.
  changed |= ImGui::DragFloat("Intensity", &desc.intensity, 0.01f, 0.f, 10.f);

  // Cast shadow.
  changed |= ImGui::Checkbox("Cast shadow", &desc.cast_shadow);

  // Shadow resolution.
  changed |= ImGui::InputInt("Shadow resolution", &desc.shadow_resolution);

  // Shadow bias.
  changed |= ImGui::DragFloat("Shadow bias", &desc.shadow_bias, 0.0001f, 0.f,
                               0.1f, "%.4f");

  return changed;
}

}  // namespace editor
