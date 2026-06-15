#include "editor/tools/SelectionTool.h"

#include <memory>

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

  game::GameObject* selected = scene_->GetSelectedObject();
  if (selected && scene_->IsDynamic(selected)) {
    LOG_F(INFO, "Deleting object '%s'", selected->GetName().c_str());
    if (history_)
      history_->Push(std::make_unique<DeleteObjectCommand>(scene_, selected));
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
    PickObjectAt(ctx, ImGui::GetMousePos(), image_pos, image_size);
  }

  // Orange wireframe bounding box for the selected object.
  if (ctx.scene->GetSelectedObject())
    DrawSelectedBBox(ctx, ImGui::GetWindowDrawList(), image_pos, image_size);
}

}  // namespace editor
