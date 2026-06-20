#include "editor/ObjectsPanel.h"

#include <algorithm>
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
#include "game/GameObjectType.h"
#include "game/GamePivot.h"
#include "game/GameObject.h"

namespace editor {

namespace {

constexpr ImGuiTreeNodeFlags kRootFlags = ImGuiTreeNodeFlags_DefaultOpen;
constexpr ImGuiTreeNodeFlags kLeafFlags =
    ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

constexpr ImVec4 kSelectedColor{0.184f, 0.769f, 0.698f, 0.35f};

}  // namespace

void ObjectsPanel::SetCommandHistory(EditorCommandHistory* history) {
  history_ = history;
}

// static
const char* ObjectsPanel::IconForType(game::GameObjectType type) {
  switch (type) {
    case game::GameObjectType::kMesh:          return ICON_FA_CUBE;
    case game::GameObjectType::kLight:         return ICON_FA_LIGHTBULB;
    case game::GameObjectType::kCamera:        return ICON_FA_CAMERA;
    case game::GameObjectType::kPivot:          return ICON_FA_LOCATION_CROSSHAIRS;
    case game::GameObjectType::kPlayerStart:   return ICON_FA_FLAG;
    case game::GameObjectType::kParticleSystem: return ICON_FA_FIRE;
    case game::GameObjectType::kSoundEmitter:  return ICON_FA_VOLUME_HIGH;
    default:                                   return ICON_FA_CIRCLE;
  }
}

void ObjectsPanel::RenderObjectLeaf(const char* icon,
                                    game::GameObject* obj,
                                    EditorScene& scene) {
  const bool is_selected = scene.IsSelected(obj);
  if (is_selected)
    ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);

  ImGui::PushID(obj);

  if (renaming_obj_ == obj) {
    if (rename_focus_needed_) {
      ImGui::SetKeyboardFocusHere();
      rename_focus_needed_ = false;
    }
    const std::string old_name = obj->GetName();
    const bool entered = ImGui::InputText("##rename", rename_buf_,
                                          sizeof(rename_buf_),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    if (entered) {
      const std::string new_name(rename_buf_);
      if (!new_name.empty() && new_name != old_name && history_)
        history_->Push(std::make_unique<RenameObjectCommand>(obj, old_name, new_name));
      renaming_obj_ = nullptr;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      renaming_obj_ = nullptr;
    } else if (ImGui::IsItemDeactivated()) {
      const std::string new_name(rename_buf_);
      if (!new_name.empty() && new_name != old_name && history_)
        history_->Push(std::make_unique<RenameObjectCommand>(obj, old_name, new_name));
      renaming_obj_ = nullptr;
    }
  } else {
    ImGuiTreeNodeFlags flags = kLeafFlags;
    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;

    const std::string leaf_label = std::string(icon) + " " + obj->GetName();
    ImGui::TreeNodeEx(leaf_label.c_str(), flags);
    if (ImGui::IsItemClicked()) {
      scene.SetSelectedObject(obj);
      if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        renaming_obj_ = obj;
        std::strncpy(rename_buf_, obj->GetName().c_str(), sizeof(rename_buf_) - 1);
        rename_buf_[sizeof(rename_buf_) - 1] = '\0';
        rename_focus_needed_ = true;
      }
    }
  }

  ImGui::PopID();

  if (is_selected)
    ImGui::PopStyleColor();
}

void ObjectsPanel::RenderPivotNode(game::GamePivot* pivot, EditorScene& scene) {
  const bool is_selected = scene.IsSelected(pivot);
  if (is_selected)
    ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);

  ImGui::PushID(pivot);

  const bool is_expanded = scene.IsPivotExpanded(pivot);
  const char* folder_icon = is_expanded ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER;
  const std::string header = std::string(folder_icon) + " " + pivot->GetName();

  ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                  ImGuiTreeNodeFlags_OpenOnDoubleClick;
  if (is_selected) node_flags |= ImGuiTreeNodeFlags_Selected;

  // Force ImGui to reflect our stored expand state.
  ImGui::SetNextItemOpen(is_expanded, ImGuiCond_Always);
  const bool open = ImGui::TreeNodeEx(header.c_str(), node_flags);

  // Sync expand state when the user clicks the arrow.
  if (ImGui::IsItemToggledOpen())
    scene.SetPivotExpanded(pivot, open);

  // Clicking the header (not the arrow) selects the pivot.
  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    scene.SetSelectedObject(pivot);

  if (is_selected)
    ImGui::PopStyleColor();

  if (open) {
    for (game::GameObject* child : pivot->GetChildren()) {
      if (child->GetType() == game::GameObjectType::kPivot) {
        RenderPivotNode(static_cast<game::GamePivot*>(child), scene);
      } else {
        RenderObjectLeaf(IconForType(child->GetType()), child, scene);
      }
    }
    ImGui::TreePop();
  }

  ImGui::PopID();
}

// static
void ObjectsPanel::RenderTypeGroup(const char* icon, const char* group_name,
                                   game::GameObjectType type,
                                   const std::vector<game::GameObject*>& objects,
                                   EditorScene& scene,
                                   game::GameObject*& renaming_obj,
                                   char* rename_buf, std::size_t rename_buf_size,
                                   bool& rename_focus_needed,
                                   EditorCommandHistory* history) {
  // Skip the category if there are no root-level objects of this type.
  const bool has_any = std::any_of(objects.begin(), objects.end(),
      [type](const game::GameObject* obj) {
        return obj->GetType() == type && obj->GetParent() == nullptr;
      });
  if (!has_any) return;

  const std::string header_str = std::string(icon) + " " + group_name;
  if (!ImGui::TreeNodeEx(header_str.c_str(), kRootFlags)) return;

  for (game::GameObject* obj : objects) {
    if (obj->GetType() != type) continue;
    if (obj->GetParent() != nullptr) continue;  // shown under its pivot parent

    const bool is_selected = scene.IsSelected(obj);
    if (is_selected)
      ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);

    ImGui::PushID(obj);

    if (renaming_obj == obj) {
      if (rename_focus_needed) {
        ImGui::SetKeyboardFocusHere();
        rename_focus_needed = false;
      }
      const std::string old_name = obj->GetName();
      const bool entered = ImGui::InputText("##rename", rename_buf,
                                            rename_buf_size,
                                            ImGuiInputTextFlags_EnterReturnsTrue);
      if (entered) {
        const std::string new_name(rename_buf);
        if (!new_name.empty() && new_name != old_name && history)
          history->Push(std::make_unique<RenameObjectCommand>(obj, old_name, new_name));
        renaming_obj = nullptr;
      } else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        renaming_obj = nullptr;
      } else if (ImGui::IsItemDeactivated()) {
        const std::string new_name(rename_buf);
        if (!new_name.empty() && new_name != old_name && history)
          history->Push(std::make_unique<RenameObjectCommand>(obj, old_name, new_name));
        renaming_obj = nullptr;
      }
    } else {
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

  // Render root-level pivot subtrees first.
  for (game::GameObject* obj : objects) {
    if (obj->GetParent() != nullptr) continue;
    if (obj->GetType() != game::GameObjectType::kPivot) continue;
    RenderPivotNode(static_cast<game::GamePivot*>(obj), scene);
  }

  // Render non-pivot root-level objects in their type categories.
  RenderTypeGroup(ICON_FA_CUBE,      "Meshes",           game::GameObjectType::kMesh,
                  objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
                  rename_focus_needed_, history_);
  RenderTypeGroup(ICON_FA_LIGHTBULB, "Lights",           game::GameObjectType::kLight,
                  objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
                  rename_focus_needed_, history_);
  RenderTypeGroup(ICON_FA_CAMERA,    "Cameras",          game::GameObjectType::kCamera,
                  objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
                  rename_focus_needed_, history_);
  RenderTypeGroup(ICON_FA_FLAG,      "Player Starts",    game::GameObjectType::kPlayerStart,
                  objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
                  rename_focus_needed_, history_);
  RenderTypeGroup(ICON_FA_FIRE,        "Particle Systems", game::GameObjectType::kParticleSystem,
                  objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
                  rename_focus_needed_, history_);
  RenderTypeGroup(ICON_FA_VOLUME_HIGH, "Sound Emitters",   game::GameObjectType::kSoundEmitter,
                  objects, scene, renaming_obj_, rename_buf_, sizeof(rename_buf_),
                  rename_focus_needed_, history_);
}

}  // namespace editor
