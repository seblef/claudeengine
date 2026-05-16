#include "editor/ResourcesPanel.h"

#include <string>

#include <IconsFontAwesome6.h>
#include <imgui.h>

#include "game/GameMaterial.h"
#include "game/MeshTemplate.h"

namespace editor {

namespace {

constexpr ImGuiTreeNodeFlags kRootFlags = ImGuiTreeNodeFlags_DefaultOpen;
constexpr ImGuiTreeNodeFlags kLeafFlags =
    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

}  // namespace

void ResourcesPanel::Render() {
  // ---- Materials -------------------------------------------------------
  const std::string mat_header = std::string(ICON_FA_PALETTE) + " Materials";
  if (ImGui::TreeNodeEx(mat_header.c_str(), kRootFlags)) {
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
  const std::string mesh_header = std::string(ICON_FA_CUBE) + " Meshes";
  if (ImGui::TreeNodeEx(mesh_header.c_str(), kRootFlags)) {
    for (const auto& kv : game::MeshTemplate::GetAll()) {
      const std::string label = std::string(ICON_FA_CUBE) + " " + kv.first;
      ImGui::TreeNodeEx(label.c_str(), kLeafFlags);
    }
    ImGui::TreePop();
  }
}

}  // namespace editor
