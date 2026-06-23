#include "editor/OutlinerPanel.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <IconsFontAwesome6.h>
#include <imgui.h>

#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/commands/AddChildPivotCommand.h"
#include "editor/commands/DeleteSubtreeCommand.h"
#include "editor/commands/GroupUnderPivotCommand.h"
#include "editor/commands/RenameObjectCommand.h"
#include "editor/commands/ReparentCommand.h"
#include "game/GameObjectType.h"
#include "game/GamePivot.h"
#include "game/GameObject.h"

namespace editor {

namespace {

constexpr ImVec4 kSelectedColor{0.184f, 0.769f, 0.698f, 0.35f};
constexpr const char* kDragDropType = "OUTLINER_OBJ";

}  // namespace

void OutlinerPanel::SetCommandHistory(EditorCommandHistory* history) {
  history_ = history;
}

// static
const char* OutlinerPanel::IconForType(game::GameObjectType type) {
  switch (type) {
    case game::GameObjectType::kMesh:           return ICON_FA_CUBE;
    case game::GameObjectType::kLight:          return ICON_FA_LIGHTBULB;
    case game::GameObjectType::kCamera:         return ICON_FA_CAMERA;
    case game::GameObjectType::kPivot:          return ICON_FA_LOCATION_CROSSHAIRS;
    case game::GameObjectType::kPlayerStart:    return ICON_FA_FLAG;
    case game::GameObjectType::kParticleSystem: return ICON_FA_FIRE;
    case game::GameObjectType::kSoundEmitter:   return ICON_FA_VOLUME_HIGH;
    case game::GameObjectType::kVehicle:        return ICON_FA_CAR;
    default:                                    return ICON_FA_CIRCLE;
  }
}

// static
std::string OutlinerPanel::MakePivotName(const EditorScene& scene) {
  const auto& objs = scene.GetObjects();
  int idx = 1;
  while (true) {
    const std::string candidate = "Pivot " + std::to_string(idx);
    const bool taken = std::any_of(objs.begin(), objs.end(),
        [&candidate](const game::GameObject* o) {
          return o->GetName() == candidate;
        });
    if (!taken) return candidate;
    ++idx;
  }
}

void OutlinerPanel::RenderRenameField(game::GameObject* obj) {
  if (rename_focus_needed_) {
    ImGui::SetKeyboardFocusHere();
    rename_focus_needed_ = false;
  }
  const std::string old_name = obj->GetName();
  const bool entered = ImGui::InputText("##rename", rename_buf_,
                                        sizeof(rename_buf_),
                                        ImGuiInputTextFlags_EnterReturnsTrue);
  const bool cancel     = ImGui::IsKeyPressed(ImGuiKey_Escape);
  const bool lost_focus = ImGui::IsItemDeactivated();

  if (entered || (!cancel && lost_focus)) {
    const std::string new_name(rename_buf_);
    if (!new_name.empty() && new_name != old_name && history_)
      history_->Push(std::make_unique<RenameObjectCommand>(obj, old_name, new_name));
    renaming_obj_ = nullptr;
  } else if (cancel) {
    renaming_obj_ = nullptr;
  }
}

// static
game::GameObject* OutlinerPanel::HandleDragDrop(game::GameObject* obj) {
  if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
    ImGui::SetDragDropPayload(kDragDropType, &obj, sizeof(obj));
    ImGui::Text("%s %s", IconForType(obj->GetType()), obj->GetName().c_str());
    ImGui::EndDragDropSource();
  }

  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload =
            ImGui::AcceptDragDropPayload(kDragDropType)) {
      game::GameObject* dragged =
          *static_cast<game::GameObject**>(const_cast<void*>(payload->Data));
      ImGui::EndDragDropTarget();
      return dragged;
    }
    ImGui::EndDragDropTarget();
  }
  return nullptr;
}

void OutlinerPanel::RenderNodeContextMenu(game::GameObject* obj,
                                          EditorScene& scene) {
  if (!ImGui::BeginPopupContextItem()) return;

  const bool multi = scene.GetSelection().size() > 1;

  if (ImGui::MenuItem("Rename")) {
    renaming_obj_ = obj;
    std::strncpy(rename_buf_, obj->GetName().c_str(), sizeof(rename_buf_) - 1);
    rename_buf_[sizeof(rename_buf_) - 1] = '\0';
    rename_focus_needed_ = true;
  }

  if (ImGui::MenuItem("Add child pivot")) {
    if (history_)
      history_->Push(std::make_unique<AddChildPivotCommand>(
          &scene, MakePivotName(scene), obj));
  }

  if (obj->GetParent() && ImGui::MenuItem("Unparent")) {
    if (history_)
      history_->Push(std::make_unique<ReparentCommand>(obj, nullptr));
  }

  if (ImGui::MenuItem("Delete subtree")) {
    confirm_delete_    = obj;
    open_delete_modal_ = true;
  }

  if (multi && ImGui::MenuItem("Group selection")) {
    if (history_) {
      history_->Push(std::make_unique<GroupUnderPivotCommand>(
          &scene, MakePivotName(scene),
          std::vector<game::GameObject*>(scene.GetSelection())));
    }
  }

  ImGui::EndPopup();
}

void OutlinerPanel::RenderNode(game::GameObject* obj, EditorScene& scene) {
  const bool is_pivot     = (obj->GetType() == game::GameObjectType::kPivot);
  auto*      pivot        = is_pivot ? static_cast<game::GamePivot*>(obj) : nullptr;
  const bool has_children = !obj->GetChildren().empty();
  const bool is_selected  = scene.IsSelected(obj);

  ImGui::PushID(obj);

  if (renaming_obj_ == obj) {
    RenderRenameField(obj);
    ImGui::PopID();
    return;
  }

  if (is_selected)
    ImGui::PushStyleColor(ImGuiCol_Header, kSelectedColor);

  const char* icon = (is_pivot && has_children)
      ? (scene.IsPivotExpanded(pivot) ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER)
      : IconForType(obj->GetType());
  const std::string label = std::string(icon) + " " + obj->GetName();

  game::GameObject* drop_target = nullptr;

  if (is_pivot && has_children) {
    const bool is_expanded = scene.IsPivotExpanded(pivot);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;
    ImGui::SetNextItemOpen(is_expanded, ImGuiCond_Always);

    const bool open = ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemToggledOpen())
      scene.SetPivotExpanded(pivot, open);

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
      if (ImGui::GetIO().KeyCtrl) {
        if (scene.IsSelected(obj)) scene.RemoveFromSelection(obj);
        else                       scene.AddToSelection(obj);
      } else {
        scene.SetSelectedObject(obj);
      }
    }

    drop_target = HandleDragDrop(obj);
    RenderNodeContextMenu(obj, scene);

    if (open) {
      const std::vector<game::GameObject*> children = obj->GetChildren();
      for (auto* child : children)
        RenderNode(child, scene);
      ImGui::TreePop();
    }
  } else {
    // Leaf: non-pivot, or pivot with no children (rendered flat).
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf |
                               ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (is_selected) flags |= ImGuiTreeNodeFlags_Selected;
    ImGui::TreeNodeEx(label.c_str(), flags);

    if (ImGui::IsItemClicked()) {
      if (ImGui::GetIO().KeyCtrl) {
        if (scene.IsSelected(obj)) scene.RemoveFromSelection(obj);
        else                       scene.AddToSelection(obj);
      } else {
        scene.SetSelectedObject(obj);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
          renaming_obj_ = obj;
          std::strncpy(rename_buf_, obj->GetName().c_str(),
                       sizeof(rename_buf_) - 1);
          rename_buf_[sizeof(rename_buf_) - 1] = '\0';
          rename_focus_needed_ = true;
        }
      }
    }

    drop_target = HandleDragDrop(obj);
    RenderNodeContextMenu(obj, scene);
  }

  if (is_selected)
    ImGui::PopStyleColor();

  // Guard against reparenting an object under itself or its current parent.
  if (drop_target && drop_target != obj && drop_target != obj->GetParent() &&
      history_) {
    history_->Push(std::make_unique<ReparentCommand>(drop_target, obj));
  }

  ImGui::PopID();
}

void OutlinerPanel::Render(EditorScene& scene) {
  // --- Tree (DFS pre-order, root-level objects only at top level) ---
  const std::vector<game::GameObject*>& objects = scene.GetObjects();
  for (game::GameObject* obj : objects) {
    if (obj->GetParent() != nullptr) continue;
    RenderNode(obj, scene);
  }

  // --- Drop-to-root target covering the remaining empty space ---
  const float avail_h = ImGui::GetContentRegionAvail().y;
  const float avail_w = ImGui::GetContentRegionAvail().x;
  if (avail_h > 0.f && avail_w > 0.f) {
    ImGui::InvisibleButton("##root_drop", ImVec2(avail_w, avail_h));
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload =
              ImGui::AcceptDragDropPayload(kDragDropType)) {
        game::GameObject* dragged =
            *static_cast<game::GameObject**>(
                const_cast<void*>(payload->Data));
        if (dragged->GetParent() && history_)
          history_->Push(std::make_unique<ReparentCommand>(dragged, nullptr));
      }
      ImGui::EndDragDropTarget();
    }
  }

  // --- Empty-area context menu ---
  if (ImGui::BeginPopupContextWindow("##outliner_bg",
                                     ImGuiPopupFlags_MouseButtonRight |
                                     ImGuiPopupFlags_NoOpenOverItems)) {
    if (ImGui::MenuItem("New pivot")) {
      if (history_)
        history_->Push(std::make_unique<AddChildPivotCommand>(
            &scene, MakePivotName(scene), nullptr));
    }
    ImGui::EndPopup();
  }

  // --- Delete-subtree confirmation modal ---
  if (open_delete_modal_) {
    ImGui::OpenPopup("Delete subtree?##confirm");
    open_delete_modal_ = false;
  }

  if (ImGui::BeginPopupModal("Delete subtree?##confirm", nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize)) {
    if (confirm_delete_) {
      ImGui::Text("Delete \"%s\" and all its descendants?",
                  confirm_delete_->GetName().c_str());
    }
    ImGui::Separator();
    if (ImGui::Button("Delete", ImVec2(100.f, 0.f))) {
      if (confirm_delete_ && history_)
        history_->Push(
            std::make_unique<DeleteSubtreeCommand>(&scene, confirm_delete_));
      confirm_delete_ = nullptr;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.f, 0.f))) {
      confirm_delete_ = nullptr;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

}  // namespace editor
