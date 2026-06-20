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
  scene_              = nullptr;
  history_            = nullptr;
  hovered_object_     = nullptr;
  last_hover_mouse_pos_ = {-1.f, -1.f};
}

void SelectionTool::OnEvent(const core::Event& event) {
  if (ImGui::GetIO().WantCaptureKeyboard) return;
  if (event.type != core::EventType::kKeyDown) return;
  if (event.key  != core::Key::kDelete) return;
  if (ImGuizmo::IsUsing()) return;
  if (!scene_) return;

  // Collect deletable (dynamic) objects from the current selection.
  const auto& sel = scene_->GetSelection();
  std::vector<game::GameObject*> to_delete;
  std::copy_if(sel.begin(), sel.end(), std::back_inserter(to_delete),
               [this](const game::GameObject* o) { return scene_->IsDynamic(o); });

  for (game::GameObject* obj : to_delete) {
    if (history_)
      history_->Push(std::make_unique<DeleteObjectCommand>(scene_, obj));
  }
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

  // Hover bbox: white semi-transparent wireframe around the object under the
  // cursor, only when it is not already selected. Ray cast is skipped when the
  // mouse has not moved since the last frame.
  const ImVec2 mouse_pos = ImGui::GetMousePos();
  const bool hoverable = ImGui::IsWindowHovered() &&
                         !ImGuizmo::IsOver() &&
                         !ImGuizmo::IsUsing();
  if (!hoverable) {
    hovered_object_ = nullptr;
  } else if (mouse_pos.x != last_hover_mouse_pos_.x ||
             mouse_pos.y != last_hover_mouse_pos_.y) {
    last_hover_mouse_pos_ = mouse_pos;
    hovered_object_ = PickHitAt(ctx, mouse_pos, image_pos, image_size);
  }
  if (hovered_object_ && !ctx.scene->IsSelected(hovered_object_))
    DrawHoverBBox(ctx, ImGui::GetWindowDrawList(), image_pos, image_size,
                  hovered_object_);

  // Orange wireframe bounding boxes for all selected objects.
  if (!ctx.scene->GetSelection().empty())
    DrawSelectedBBox(ctx, ImGui::GetWindowDrawList(), image_pos, image_size);
}

}  // namespace editor
