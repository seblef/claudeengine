#include "editor/ResourcesPanel.h"

#include "editor/EditorScene.h"
#include "game/MeshTemplate.h"

#include <imgui.h>

namespace editor {

namespace {

constexpr ImVec2 kIconSize = {12.f, 12.f};
constexpr ImGuiColorEditFlags kIconFlags =
    ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip |
    ImGuiColorEditFlags_NoDragDrop;
constexpr ImGuiTreeNodeFlags kRootFlags = ImGuiTreeNodeFlags_DefaultOpen;
constexpr ImGuiTreeNodeFlags kLeafFlags =
    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

}  // namespace

void ResourcesPanel::Render(const EditorScene& scene) {
  // ---- Materials -------------------------------------------------------
  const ImVec4 kMatColor{220.f / 255.f, 80.f / 255.f, 220.f / 255.f, 1.f};
  ImGui::ColorButton("##mat_icon", kMatColor, kIconFlags, kIconSize);
  ImGui::SameLine();
  if (ImGui::TreeNodeEx("Materials", kRootFlags)) {
    for (const auto& kv : scene.GetMaterials())
      ImGui::TreeNodeEx(kv.first.c_str(), kLeafFlags);
    ImGui::TreePop();
  }

  // ---- Mesh templates --------------------------------------------------
  const ImVec4 kMeshColor{80.f / 255.f, 200.f / 255.f, 220.f / 255.f, 1.f};
  ImGui::ColorButton("##mesh_icon", kMeshColor, kIconFlags, kIconSize);
  ImGui::SameLine();
  if (ImGui::TreeNodeEx("Meshes", kRootFlags)) {
    for (const auto& kv : game::MeshTemplate::GetAll())
      ImGui::TreeNodeEx(kv.first.c_str(), kLeafFlags);
    ImGui::TreePop();
  }
}

}  // namespace editor
