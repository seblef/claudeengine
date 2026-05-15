#include "editor/ObjectsPanel.h"

#include "editor/EditorScene.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"

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

constexpr ImVec4 kSelectedColor{0.26f, 0.59f, 0.98f, 0.55f};

void RenderGroup(const char* btn_id, const char* group_name,
                 const ImVec4& icon_color, game::GameObjectType type,
                 const std::vector<game::GameObject*>& objects,
                 EditorScene& scene) {
  ImGui::ColorButton(btn_id, icon_color, kIconFlags, kIconSize);
  ImGui::SameLine();
  if (!ImGui::TreeNodeEx(group_name, kRootFlags)) return;

  const game::GameObject* selected = scene.GetSelectedObject();
  for (game::GameObject* obj : objects) {
    if (obj->GetType() != type) continue;

    const bool is_selected = (obj == selected);
    if (is_selected) {
      ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);
    }

    ImGuiTreeNodeFlags flags = kLeafFlags;
    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(obj);
    ImGui::TreeNodeEx(obj->GetName().c_str(), flags);
    if (ImGui::IsItemClicked()) {
      scene.SetSelectedObject(obj);
    }
    ImGui::PopID();

    if (is_selected) {
      ImGui::PopStyleColor();
    }
  }

  ImGui::TreePop();
}

}  // namespace

void ObjectsPanel::Render(EditorScene& scene) {
  const std::vector<game::GameObject*>& objects = scene.GetObjects();

  const ImVec4 kMeshColor{1.f, 165.f / 255.f, 0.f, 1.f};
  RenderGroup("##mesh_icon", "Meshes", kMeshColor,
              game::GameObjectType::kMesh, objects, scene);

  const ImVec4 kLightColor{1.f, 220.f / 255.f, 50.f / 255.f, 1.f};
  RenderGroup("##light_icon", "Lights", kLightColor,
              game::GameObjectType::kLight, objects, scene);

  const ImVec4 kCameraColor{100.f / 255.f, 180.f / 255.f, 1.f, 1.f};
  RenderGroup("##camera_icon", "Cameras", kCameraColor,
              game::GameObjectType::kCamera, objects, scene);
}

}  // namespace editor
