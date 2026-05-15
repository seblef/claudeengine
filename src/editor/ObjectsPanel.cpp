#include "editor/ObjectsPanel.h"

#include "editor/EditorScene.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"

#include <string>

#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace editor {

namespace {

constexpr ImGuiTreeNodeFlags kRootFlags = ImGuiTreeNodeFlags_DefaultOpen;
constexpr ImGuiTreeNodeFlags kLeafFlags =
    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

constexpr ImVec4 kSelectedColor{0.26f, 0.59f, 0.98f, 0.55f};

void RenderGroup(const char* icon, const char* group_name,
                 game::GameObjectType type,
                 const std::vector<game::GameObject*>& objects,
                 EditorScene& scene) {
  const std::string header = std::string(icon) + " " + group_name;
  if (!ImGui::TreeNodeEx(header.c_str(), kRootFlags)) return;

  const game::GameObject* selected = scene.GetSelectedObject();
  for (game::GameObject* obj : objects) {
    if (obj->GetType() != type) continue;

    const bool is_selected = (obj == selected);
    if (is_selected)
      ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);

    ImGuiTreeNodeFlags flags = kLeafFlags;
    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;

    ImGui::PushID(obj);
    const std::string leaf_label = std::string(icon) + " " + obj->GetName();
    ImGui::TreeNodeEx(leaf_label.c_str(), flags);
    if (ImGui::IsItemClicked())
      scene.SetSelectedObject(obj);
    ImGui::PopID();

    if (is_selected)
      ImGui::PopStyleColor();
  }

  ImGui::TreePop();
}

}  // namespace

void ObjectsPanel::Render(EditorScene& scene) {
  const std::vector<game::GameObject*>& objects = scene.GetObjects();
  RenderGroup(ICON_FA_CUBE,      "Meshes",  game::GameObjectType::kMesh,   objects, scene);
  RenderGroup(ICON_FA_LIGHTBULB, "Lights",  game::GameObjectType::kLight,  objects, scene);
  RenderGroup(ICON_FA_CAMERA,    "Cameras", game::GameObjectType::kCamera, objects, scene);
}

}  // namespace editor
