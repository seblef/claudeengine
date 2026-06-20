#include "editor/tools/TransformTool.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_set>
#include <vector>

#include "core/BBox3.h"
#include "core/Camera.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/PickingAccelerator.h"
#include "editor/commands/MultiTransformCommand.h"
#include "editor/commands/TransformCommand.h"
#include "editor/tools/EditorToolContext.h"
#include "editor/tools/PickingUtils.h"
#include "game/GameCamera.h"
#include "game/GameObject.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace editor {

TransformTool::TransformTool(ImGuizmo::OPERATION op) : op_(op) {}

void TransformTool::OnDeactivate() {
  hovered_object_       = nullptr;
  last_hover_mouse_pos_ = {-1.f, -1.f};
}

void TransformTool::OnRender(const EditorToolContext& ctx,
                              ImVec2 image_pos, ImVec2 image_size) {
  if (!ctx.scene || !ctx.camera) return;

  // Hover bbox: white semi-transparent wireframe around the object under the
  // cursor, only when it is not already selected. Ray cast is skipped when the
  // gizmo is active or the mouse has not moved since the last frame. Runs
  // unconditionally so it is also visible with an empty selection.
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

  const std::vector<game::GameObject*>& selection = ctx.scene->GetSelection();
  if (selection.empty()) {
    gizmo_was_using_ = false;
    drag_objects_.clear();
    drag_before_.clear();

    // Allow picking a different object when clicking outside the gizmo.
    if (ImGui::IsWindowHovered() &&
        !ImGuizmo::IsViewManipulateHovered() &&
        !ImGuizmo::IsOver() &&
        !ImGui::GetIO().KeyAlt &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
      PickObjectAt(ctx, ImGui::GetMousePos(), image_pos, image_size,
                   ImGui::GetIO().KeyCtrl);
    }
    return;
  }

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

  const bool was_using = gizmo_was_using_;

  if (selection.size() == 1) {
    // Single-object path: gizmo placed at the object's own transform.
    game::GameObject* obj = selection[0];
    const core::Mat4f model_t = obj->GetWorldTransform().Transpose();
    float model_im[16];
    std::memcpy(model_im, model_t.Data(), sizeof(model_im));

    ImGuizmo::Manipulate(view_im, proj_im, op_, ImGuizmo::WORLD, model_im);

    const bool gizmo_using = ImGuizmo::IsUsing();

    if (!gizmo_was_using_ && gizmo_using) {
      // Drag start: snapshot the single object.
      drag_objects_ = { obj };
      drag_before_  = { obj->GetWorldTransform() };
      pivot_center_ = obj->GetWorldBBox().GetCenter();
    }

    if (gizmo_using) {
      const core::Mat4f model_t_after(model_im);
      obj->SetWorldTransform(model_t_after.Transpose());
    }

    if (gizmo_was_using_ && !gizmo_using) {
      if (ctx.picking_acc) ctx.picking_acc->UpdateMoved(obj);
      if (ctx.history && !drag_before_.empty()) {
        const core::Mat4f after = obj->GetWorldTransform();
        if (after != drag_before_[0]) {
          ctx.history->Push(std::make_unique<TransformCommand>(
              obj, drag_before_[0], after));
        }
      }
      drag_objects_.clear();
      drag_before_.clear();
    }

    gizmo_was_using_ = gizmo_using;
  } else {
    // Multi-object path: gizmo placed at the centre of the combined bbox.
    core::BBox3 combined = selection[0]->GetWorldBBox();
    for (std::size_t i = 1; i < selection.size(); ++i)
      combined << selection[i]->GetWorldBBox();
    const core::Vec3f center = combined.GetCenter();

    // During a drag we pass the accumulated pivot so that ImGuizmo builds
    // each frame's delta on top of the previous state — without this, the
    // rotation part of the gizmo resets to identity orientation every frame
    // and each frame only the per-frame angular delta is applied instead of
    // the cumulative rotation since drag start.
    // Outside a drag we always position the gizmo at the current centre.
    float model_im[16];
    if (gizmo_was_using_) {
      std::memcpy(model_im, pivot_im_, sizeof(model_im));
    } else {
      const core::Mat4f pivot_t = core::Mat4f::Translation(center).Transpose();
      std::memcpy(model_im, pivot_t.Data(), sizeof(model_im));
    }

    ImGuizmo::Manipulate(view_im, proj_im, op_, ImGuizmo::WORLD, model_im);

    const bool gizmo_using = ImGuizmo::IsUsing();

    if (!gizmo_was_using_ && gizmo_using) {
      // Drag start: snapshot objects, pivot centre, and current model_im.
      // Filter out objects whose parent is already in the selection; their
      // transform propagates automatically via the parent-child hierarchy.
      pivot_center_ = center;
      std::memcpy(pivot_im_, model_im, sizeof(pivot_im_));
      std::unordered_set<const game::GameObject*> sel_set(
          selection.begin(), selection.end());
      drag_objects_.clear();
      drag_before_.clear();
      for (game::GameObject* o : selection) {
        if (sel_set.count(o->GetParent()) == 0) {
          drag_objects_.push_back(o);
          drag_before_.push_back(o->GetWorldTransform());
        }
      }
    }

    if (gizmo_using && !drag_objects_.empty()) {
      // Store the updated pivot for the next frame.
      std::memcpy(pivot_im_, model_im, sizeof(pivot_im_));

      // Apply: new_T[i] = pivot_after * T(-centre_init) * T_before[i].
      const core::Mat4f pivot_after = core::Mat4f(model_im).Transpose();
      const core::Mat4f pivot_before_inv =
          core::Mat4f::Translation(-pivot_center_);

      for (std::size_t i = 0; i < drag_objects_.size(); ++i) {
        const core::Mat4f new_t =
            pivot_after * pivot_before_inv * drag_before_[i];
        drag_objects_[i]->SetWorldTransform(new_t);
      }
    }

    if (gizmo_was_using_ && !gizmo_using) {
      if (!drag_objects_.empty()) {
        std::vector<MultiTransformCommand::Entry> entries;
        entries.reserve(drag_objects_.size());
        bool any_changed = false;
        for (std::size_t i = 0; i < drag_objects_.size(); ++i) {
          const core::Mat4f after = drag_objects_[i]->GetWorldTransform();
          if (after != drag_before_[i]) any_changed = true;
          entries.push_back({drag_objects_[i], drag_before_[i], after});
          if (ctx.picking_acc) ctx.picking_acc->UpdateMoved(drag_objects_[i]);
        }
        if (ctx.history && any_changed)
          ctx.history->Push(
              std::make_unique<MultiTransformCommand>(std::move(entries)));
      }
      drag_objects_.clear();
      drag_before_.clear();
    }

    gizmo_was_using_ = gizmo_using;
  }

  // Allow picking a different object when clicking outside the gizmo.
  // Guard against the drag-end frame where LMB is released after a gizmo drag.
  if (ImGui::IsWindowHovered() &&
      !ImGuizmo::IsViewManipulateHovered() &&
      !ImGuizmo::IsOver() &&
      !was_using &&
      !ImGui::GetIO().KeyAlt &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    PickObjectAt(ctx, ImGui::GetMousePos(), image_pos, image_size,
                 ImGui::GetIO().KeyCtrl);
  }

  // Orange wireframe bounding boxes for all selected objects.
  if (!ctx.scene->GetSelection().empty())
    DrawSelectedBBox(ctx, ImGui::GetWindowDrawList(), image_pos, image_size);
}

bool TransformTool::IsCapturingMouse() const {
  return ImGuizmo::IsUsing();
}

}  // namespace editor
