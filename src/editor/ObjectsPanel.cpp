#include "editor/ObjectsPanel.h"

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <IconsFontAwesome6.h>
#include <imgui.h>

#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/commands/RenameObjectCommand.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"

namespace editor {

namespace {

constexpr ImGuiTreeNodeFlags kRootFlags = ImGuiTreeNodeFlags_DefaultOpen;
constexpr ImGuiTreeNodeFlags kLeafFlags =
    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

constexpr ImVec4 kSelectedColor{0.184f, 0.769f, 0.698f, 0.35f};  // teal accent #2FC4B2

}  // namespace

void ObjectsPanel::SetCommandHistory(EditorCommandHistory* history) {
  history_ = history;
}

// static
void ObjectsPanel::RenderGroup(const char* icon, const char* group_name,
                               game::GameObjectType type,
                               const std::vector<game::GameObject*>& objects,
                               EditorScene& scene,
                               game::GameObject*& renaming_obj,
                               char* rename_buf, std::size_t rename_buf_size,
                               bool& rename_focus_needed,
                               EditorCommandHistory* history) {
  const std::string header = std::string(icon) + " " + group_name;
  if (!ImGui::TreeNodeEx(header.c_str(), kRootFlags)) return;

  const game::GameObject* selected = scene.GetSelectedObject();
  for (game::GameObject* obj : objects) {
    if (obj->GetType() != type) continue;

    const bool is_selected = (obj == selected);
    if (is_selected)
      ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);

    ImGui::PushID(obj);

    if (renaming_obj == obj) {
      // Inline rename input field.
      if (rename_focus_needed) {
        ImGui::SetKeyboardFocusHere();
        rename_focus_needed = false;
      }
      const std::string old_name = obj->GetName();
      const bool entered = ImGui::InputText("##rename", rename_buf,
                                            rename_buf_size,
                                            ImGuiInputTextFlags_EnterReturnsTrue);
      if (entered) {
        // Commit on Enter.
        const std::string new_name(rename_buf);
        if (!new_name.empty() && new_name != old_name && history)
          history->Push(std::make_unique<RenameObjectCommand>(obj, old_name, new_name));
        renaming_obj = nullptr;
      } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        renaming_obj = nullptr;  // cancel without pushing a command
      } else if (ImGui::IsItemDeactivated()) {
        // Commit on focus loss. IsItemDeactivated() is used instead of
        // !IsItemActive() because InputText is not active on its first frame
        // (SetKeyboardFocusHere activates it via NavActivateId one frame later).
        const std::string new_name(rename_buf);
        if (!new_name.empty() && new_name != old_name && history)
          history->Push(std::make_unique<RenameObjectCommand>(obj, old_name, new_name));
        renaming_obj = nullptr;
      }
    } else {
      // Normal display mode.
      ImGuiTreeNodeFlags flags = kLeafFlags;
      if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;

      const std::string leaf_label = std::string(icon) + " " + obj->GetName();
      ImGui::TreeNodeEx(leaf_label.c_str(), flags);
      if (ImGui::IsItemClicked()) {
        scene.SetSelectedObject(obj);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          renaming_obj = obj;
          std::strncpy(rename_buf, obj->GetName().c_str(), rename_buf_size - 1);
          rename_buf[rename_buf_size - 1] = '\0';
          rename_focus_needed = true;
        }
      }
    }

    ImGui::PopID();

    if (is_selected)
      ImGui::PopStyleColor();
  }

  ImGui::TreePop();
}

void ObjectsPanel::Render(EditorScene& scene) {
  const std::vector<game::GameObject*>& objects = scene.GetObjects();
  RenderGroup(ICON_FA_CUBE,      "Meshes",        game::GameObjectType::kMesh,
              objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
              rename_focus_needed_, history_);
  RenderGroup(ICON_FA_LIGHTBULB, "Lights",        game::GameObjectType::kLight,
              objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
              rename_focus_needed_, history_);
  RenderGroup(ICON_FA_CAMERA,    "Cameras",       game::GameObjectType::kCamera,
              objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
              rename_focus_needed_, history_);
  RenderGroup(ICON_FA_FLAG,      "Player Starts", game::GameObjectType::kPlayerStart,
              objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
              rename_focus_needed_, history_);
  RenderGroup(ICON_FA_FIRE,     "Particle Systems", game::GameObjectType::kParticleSystem,
              objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
              rename_focus_needed_, history_);
}

}  // namespace editor
