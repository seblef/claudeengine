#include "editor/tools/TransformTool.h"

#include <cstring>
#include <memory>

#include "core/Camera.h"
#include "core/Mat4f.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/PickingAccelerator.h"
#include "editor/commands/TransformCommand.h"
#include "editor/tools/EditorToolContext.h"
#include "editor/tools/PickingUtils.h"
#include "game/GameCamera.h"
#include "game/GameObject.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace editor {

TransformTool::TransformTool(ImGuizmo::OPERATION op) : op_(op) {}

void TransformTool::OnRender(const EditorToolContext& ctx,
                              ImVec2 image_pos, ImVec2 image_size) {
  if (!ctx.scene || !ctx.camera) return;

  // ImGuizmo uses row-major storage with row-vector convention (translation in
  // last row), which is the transpose of our column-vector Mat4f convention.
  const core::Camera* cam    = ctx.camera->GetCamera();
  const core::Mat4f   view_t = cam->GetViewMatrix().Transpose();
  const core::Mat4f   proj_t = cam->GetProjectionMatrix().Transpose();
  float view_im[16], proj_im[16];
  std::memcpy(view_im, view_t.Data(), sizeof(view_im));
  std::memcpy(proj_im, proj_t.Data(), sizeof(proj_im));

  const ImVec2 vp_pos = ImGui::GetWindowPos();
  ImGuizmo::SetRect(vp_pos.x, vp_pos.y, image_size.x, image_size.y);
  ImGuizmo::SetDrawlist();

  // Snapshot previous-frame gizmo state before updating it so that the
  // pick guard below sees the correct "was dragging last frame" value.
  const bool was_using = gizmo_was_using_;

  game::GameObject* selected = ctx.scene->GetSelectedObject();
  if (selected) {
    const core::Mat4f model_t = selected->GetWorldTransform().Transpose();
    float model_im[16];
    std::memcpy(model_im, model_t.Data(), sizeof(model_im));

    ImGuizmo::Manipulate(view_im, proj_im, op_, ImGuizmo::WORLD, model_im);

    const bool gizmo_using = ImGuizmo::IsUsing();

    if (!gizmo_was_using_ && gizmo_using)
      gizmo_before_transform_ = selected->GetWorldTransform();

    if (gizmo_using) {
      const core::Mat4f model_t_after(model_im);
      selected->SetWorldTransform(model_t_after.Transpose());
    }

    if (gizmo_was_using_ && !gizmo_using) {
      if (ctx.picking_acc)
        ctx.picking_acc->UpdateMoved(selected);
      if (ctx.history) {
        const core::Mat4f after = selected->GetWorldTransform();
        if (after != gizmo_before_transform_)
          ctx.history->Push(std::make_unique<TransformCommand>(
              selected, gizmo_before_transform_, after));
      }
    }

    gizmo_was_using_ = gizmo_using;
  } else {
    gizmo_was_using_ = false;
  }

  // Allow picking a different object when clicking outside the gizmo.
  // Guard against the drag-end frame where LMB is released after a gizmo drag.
  if (ImGui::IsWindowHovered() &&
      !ImGuizmo::IsViewManipulateHovered() &&
      !ImGuizmo::IsOver() &&
      !was_using &&
      !ImGui::GetIO().KeyAlt &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    PickObjectAt(ctx, ImGui::GetMousePos(), image_pos, image_size);
  }

  // Orange wireframe bounding box for the selected object.
  if (ctx.scene->GetSelectedObject())
    DrawSelectedBBox(ctx, ImGui::GetWindowDrawList(), image_pos, image_size);
}

bool TransformTool::IsCapturingMouse() const {
  return ImGuizmo::IsUsing();
}

}  // namespace editor
