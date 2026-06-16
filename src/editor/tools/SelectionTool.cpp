#include "editor/tools/SelectionTool.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "core/Event.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/commands/DeleteObjectCommand.h"
#include "editor/tools/EditorToolContext.h"
#include "editor/tools/PickingUtils.h"
#include "game/GameObject.h"

#include <ImGuizmo.h>
#include <imgui.h>
#include <loguru.hpp>

namespace editor {

void SelectionTool::OnActivate(const EditorToolContext& ctx) {
  scene_   = ctx.scene;
  history_ = ctx.history;
}

void SelectionTool::OnDeactivate() {
  scene_   = nullptr;
  history_ = nullptr;
}

void SelectionTool::OnEvent(const core::Event& event) {
  if (ImGui::GetIO().WantCaptureKeyboard) return;
  if (event.type != core::EventType::kKeyDown) return;
  if (event.key  != core::Key::kDelete) return;
  if (ImGuizmo::IsUsing()) return;
  if (!scene_) return;

  // Capture the group before deletion: ReclaimDynamicObject strips each object
  // from its group via RemoveFromGroup, so GetSelectionGroup() would return null
  // after the first deletion.
  ObjectGroup* sel_group = scene_->GetSelectionGroup();

  // Collect deletable (dynamic) objects from the current selection.
  const auto& sel = scene_->GetSelection();
  std::vector<game::GameObject*> to_delete;
  std::copy_if(sel.begin(), sel.end(), std::back_inserter(to_delete),
               [this](const game::GameObject* o) { return scene_->IsDynamic(o); });

  for (game::GameObject* obj : to_delete) {
    LOG_F(INFO, "Deleting object '%s'", obj->GetName().c_str());
    if (history_)
      history_->Push(std::make_unique<DeleteObjectCommand>(scene_, obj));
  }

  // If the whole group was deleted (group is now empty), remove it from the scene.
  if (sel_group && sel_group->objects.empty())
    scene_->DeleteGroup(sel_group);
}

void SelectionTool::OnRender(const EditorToolContext& ctx,
                              ImVec2 image_pos, ImVec2 image_size) {
  if (!ctx.scene) return;

  // Pick on LMB release. Skipped when the gizmo is hovered (IsOver) or was
  // being dragged last frame (gizmo_was_using): the drag-end release must not
  // trigger a spurious selection change.
  if (ImGui::IsWindowHovered() &&
      !ImGuizmo::IsViewManipulateHovered() &&
      !ImGuizmo::IsOver() &&
      !ctx.gizmo_was_using &&
      !ImGui::GetIO().KeyAlt &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    PickObjectAt(ctx, ImGui::GetMousePos(), image_pos, image_size,
                 ImGui::GetIO().KeyCtrl);
  }

  // Orange wireframe bounding boxes for all selected objects.
  if (!ctx.scene->GetSelection().empty())
    DrawSelectedBBox(ctx, ImGui::GetWindowDrawList(), image_pos, image_size);
}

}  // namespace editor
