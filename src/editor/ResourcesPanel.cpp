#include "editor/ResourcesPanel.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <IconsFontAwesome6.h>
#include <imgui.h>

#include "core/Config.h"
#include "game/GameMaterial.h"
#include "game/MeshTemplate.h"
#include "particles/ParticleSystemTemplate.h"

namespace editor {

namespace {

constexpr ImGuiTreeNodeFlags kRootFlags = ImGuiTreeNodeFlags_DefaultOpen;
constexpr ImGuiTreeNodeFlags kLeafFlags =
    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

}  // namespace

void ResourcesPanel::Render() {
  // ---- Materials -------------------------------------------------------
  bool mat_open = ImGui::TreeNodeEx("##mat_header", kRootFlags,
                                    "%s Materials", ICON_FA_PALETTE);
  ImGui::SameLine(ImGui::GetWindowWidth() - 50.f);
  if (ImGui::SmallButton(ICON_FA_PLUS)) {
    std::strncpy(new_mat_name_buf_, "new_material", sizeof(new_mat_name_buf_));
    show_new_mat_modal_ = true;
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_FA_FILE_IMPORT)) {
    if (on_import_material_) on_import_material_();
  }
  if (mat_open) {
    for (const auto& kv : game::GameMaterial::GetRegistry()) {
      if (kv.first.rfind("__proc_", 0) == 0) continue;
      const std::string label = std::string(ICON_FA_PALETTE) + " " + kv.first;
      ImGui::TreeNodeEx(label.c_str(), kLeafFlags);
      if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (on_material_open_) on_material_open_(kv.second);
      }
    }
    ImGui::TreePop();
  }

  // ---- Mesh templates --------------------------------------------------
  bool mesh_open = ImGui::TreeNodeEx("##mesh_header", kRootFlags,
                                     "%s Meshes", ICON_FA_CUBE);
  ImGui::SameLine(ImGui::GetWindowWidth() - 30.f);
  if (ImGui::SmallButton(ICON_FA_FILE_IMPORT)) {
    if (on_import_mesh_) on_import_mesh_();
  }
  if (mesh_open) {
    for (const auto& kv : game::MeshTemplate::GetAll()) {
      const std::string label = std::string(ICON_FA_CUBE) + " " + kv.first;
      ImGui::TreeNodeEx(label.c_str(), kLeafFlags);
      const game::MeshTemplate* tmpl_ptr = kv.second;
      if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        ImGui::SetDragDropPayload("MESH_TEMPLATE", &tmpl_ptr, sizeof(game::MeshTemplate*));
        ImGui::Text("Place: %s", kv.first.c_str());
        ImGui::EndDragDropSource();
      }
      const std::string ctx_id = "##mesh_ctx_" + kv.first;
      if (ImGui::BeginPopupContextItem(ctx_id.c_str())) {
        if (ImGui::MenuItem("Edit\xe2\x80\xa6")) {
          if (on_mesh_edit_) on_mesh_edit_(tmpl_ptr);
        }
        ImGui::EndPopup();
      }
    }
    ImGui::TreePop();
  }

  // ---- Particle effects -----------------------------------------------
  bool fx_open = ImGui::TreeNodeEx("##fx_header", kRootFlags,
                                   "%s Particle Effects", ICON_FA_FIRE);
  if (fx_open) {
    for (const auto& kv : particles::ParticleSystemTemplate::GetAll()) {
      const std::string label = std::string(ICON_FA_FIRE) + " " + kv.first;
      ImGui::TreeNodeEx(label.c_str(), kLeafFlags);
      if (ImGui::IsItemHovered() &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (on_particle_open_) on_particle_open_(kv.first);
      }
    }
    ImGui::TreePop();
  }

  // ---- Sound templates -------------------------------------------------
  bool snd_open = ImGui::TreeNodeEx("##snd_header", kRootFlags,
                                    "%s Sounds", ICON_FA_MUSIC);
  ImGui::SameLine(ImGui::GetWindowWidth() - 30.f);
  if (ImGui::SmallButton(ICON_FA_PLUS "##snd")) {
    std::strncpy(new_sound_name_buf_, "new_sound", sizeof(new_sound_name_buf_));
    show_new_sound_modal_ = true;
  }
  if (snd_open) {
    const auto sounds_dir = core::Config::GetDataFolder() / "sounds";
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::directory_iterator(sounds_dir, ec)) {
      if (!entry.is_regular_file()) continue;
      const auto& p = entry.path();
      if (p.extension() == ".yaml" && p.stem().extension() == ".sound")
        names.push_back(p.stem().stem().string());
    }
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
      const std::string label = std::string(ICON_FA_MUSIC) + " " + name;
      ImGui::TreeNodeEx(label.c_str(), kLeafFlags);
      if (ImGui::IsItemHovered() &&
          ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (on_sound_open_) on_sound_open_(name);
      }
    }
    ImGui::TreePop();
  }

  // ---- New Material modal ----------------------------------------------
  if (show_new_mat_modal_) {
    ImGui::OpenPopup("New Material##modal");
    show_new_mat_modal_ = false;
  }
  if (ImGui::BeginPopupModal("New Material##modal", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Name", new_mat_name_buf_, sizeof(new_mat_name_buf_));
    if (ImGui::Button("Create") && std::strlen(new_mat_name_buf_) > 0) {
      if (on_new_material_) on_new_material_(new_mat_name_buf_);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  // ---- New Sound Template modal ----------------------------------------
  if (show_new_sound_modal_) {
    ImGui::OpenPopup("New Sound Template##modal");
    show_new_sound_modal_ = false;
  }
  if (ImGui::BeginPopupModal("New Sound Template##modal", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::InputText("Name", new_sound_name_buf_, sizeof(new_sound_name_buf_));
    if (ImGui::Button("Create") && std::strlen(new_sound_name_buf_) > 0) {
      if (on_new_sound_) on_new_sound_(new_sound_name_buf_);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }
}

}  // namespace editor
