#include "editor/tools/PlacementTool.h"

#include <memory>
#include <utility>

#include "core/Mat4f.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/PickingAccelerator.h"
#include "editor/commands/PlaceObjectCommand.h"
#include "editor/tools/EditorToolContext.h"
#include "editor/tools/ViewportRaycast.h"
#include "game/GameObject.h"

#include <imgui.h>
#include <loguru.hpp>

namespace editor {

PlacementTool::PlacementTool(std::unique_ptr<game::GameObject> preview,
                              float height,
                              ImGuiMouseCursor cursor,
                              std::function<void()> on_done)
    : pending_preview_(std::move(preview)),
      preview_height_(height),
      preview_cursor_(cursor),
      on_done_(std::move(on_done)) {}

void PlacementTool::OnActivate(const EditorToolContext& ctx) {
  scene_       = ctx.scene;
  picking_acc_ = ctx.picking_acc;
}

void PlacementTool::OnDeactivate() {
  if (preview_object_ && scene_)
    scene_->RemoveDynamicObject(preview_object_);
  preview_object_ = nullptr;
  pending_preview_.reset();
  scene_          = nullptr;
  picking_acc_    = nullptr;
}

void PlacementTool::UpdatePreviewPosition(const EditorToolContext& ctx,
                                           ImVec2 image_pos, ImVec2 image_size) {
  const auto hit = ComputeTerrainHit(ctx.camera, ctx.terrain_data,
                                      ImGui::GetMousePos(), image_pos, image_size);
  if (!hit) return;

  if (pending_preview_)
    preview_object_ = ctx.scene->AddDynamicObject(std::move(pending_preview_));

  if (preview_object_) {
    preview_object_->SetWorldTransform(
        core::Mat4f::Translation({hit->x, hit->y + preview_height_, hit->z}));
    if (ctx.picking_acc)
      ctx.picking_acc->UpdateMoved(preview_object_);
  }
}

void PlacementTool::PlaceObject(const EditorToolContext& ctx) {
  if (!preview_object_) return;

  if (ctx.history) {
    auto obj = ctx.scene->ReclaimDynamicObject(preview_object_);
    preview_object_ = nullptr;
    ctx.history->Push(
        std::make_unique<PlaceObjectCommand>(ctx.scene, std::move(obj)));
  } else {
    ctx.scene->SetSelectedObject(preview_object_);
    preview_object_ = nullptr;
  }

  if (on_done_) on_done_();
}

void PlacementTool::OnRender(const EditorToolContext& ctx,
                               ImVec2 image_pos, ImVec2 image_size) {
  if (!ctx.scene || !ImGui::IsWindowHovered()) return;

  ImGui::SetMouseCursor(preview_cursor_);
  UpdatePreviewPosition(ctx, image_pos, image_size);

  if (!ImGui::GetIO().KeyAlt &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    PlaceObject(ctx);
  }
}

}  // namespace editor
