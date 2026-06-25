#include "editor/tools/RoadTool.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>

#include <ImGuizmo.h>
#include <imgui.h>
#include <loguru.hpp>

#include "core/EventType.h"
#include "core/Key.h"
#include "core/Mat4f.h"
#include "core/Vec4f.h"
#include "editor/EditorScene.h"
#include "editor/ObjectNamingUtils.h"
#include "editor/tools/EditorToolContext.h"
#include "editor/tools/ViewportRaycast.h"
#include "game/GameCamera.h"
#include "game/GameObjectType.h"
#include "game/GameRoad.h"
#include "terrain/TerrainData.h"
#include "track/RoadSpline.h"

namespace editor {

namespace {

// Screen-space radius for control-point hit detection (pixels).
constexpr float kHoverRadius    = 8.f;
constexpr float kHoverRadiusSq  = kHoverRadius * kHoverRadius;

// Drawn circle radius (pixels).
constexpr float kPointRadius    = 6.f;

// Minimum control points allowed in the spline.
constexpr int   kMinPoints      = 3;

constexpr ImU32 kColDefault  = IM_COL32(180, 180, 180, 255);
constexpr ImU32 kColHovered  = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kColSelected = IM_COL32(255, 220, 0,   255);
constexpr ImU32 kColGhost    = IM_COL32(255, 220, 0,   180);

}  // namespace

// ---- OnActivate / OnDeactivate -----------------------------------------------

void RoadTool::OnActivateCreation() {
  entering_creation_mode_ = true;
}

void RoadTool::OnActivate(const EditorToolContext& ctx) {
  road_           = nullptr;
  selected_point_ = -1;
  dragging_       = false;
  hovering_       = false;
  scene_          = ctx.scene;

  if (entering_creation_mode_) {
    entering_creation_mode_ = false;
    mode_ = Mode::kCreating;
    return;
  }

  mode_ = Mode::kEditing;
  game::GameObject* sel = ctx.scene ? ctx.scene->GetSelectedObject() : nullptr;
  if (!sel || sel->GetType() != game::GameObjectType::kRoad) return;
  road_ = static_cast<game::GameRoad*>(sel);
}

void RoadTool::OnDeactivate() {
  road_           = nullptr;
  selected_point_ = -1;
  dragging_       = false;
  hovering_       = false;
  mode_           = Mode::kEditing;
  scene_          = nullptr;
  // entering_creation_mode_ is intentionally NOT reset here; it is consumed
  // by OnActivate() which is called immediately after OnDeactivate().
}

// ---- OnEvent -----------------------------------------------------------------

void RoadTool::OnEvent(const core::Event& event) {
  if (event.type != core::EventType::kKeyDown) return;

  if (mode_ == Mode::kCreating) {
    if (event.key == core::Key::kEnter) {
      if (!road_) return;
      if (road_->GetSpline().GetPointCount() < kMinPoints) return;
      // Finalise: switch to editing. EditorWindow's auto-logic detects the
      // selected road and transitions the toolbar from kCreateRoad to kRoad.
      mode_ = Mode::kEditing;
      selected_point_ = -1;
      if (scene_) scene_->SetSelectedObject(road_);
    } else if (event.key == core::Key::kEscape) {
      // Cancel: remove the road from scene if one was started.
      if (road_ && scene_) scene_->RemoveDynamicObject(road_);
      road_ = nullptr;
      mode_ = Mode::kEditing;
      // Deselecting will trigger auto-logic to switch toolbar to kSelection.
      if (scene_) scene_->SetSelectedObject(nullptr);
    }
    return;
  }

  // kEditing: Delete key removes the selected control point.
  if (!road_) return;
  if (event.key != core::Key::kDelete) return;
  if (selected_point_ < 0)             return;

  track::RoadSpline& spline = road_->GetSpline();
  if (spline.GetPointCount() <= kMinPoints) return;

  spline.RemoveControlPoint(selected_point_);
  selected_point_ = std::min(selected_point_, spline.GetPointCount() - 1);
  LOG_F(9, "RoadTool: removed control point, %d remaining",
        spline.GetPointCount());
}

// ---- Helpers -----------------------------------------------------------------

bool RoadTool::ProjectPoint(const core::Vec3f& world_pos,
                             const core::Mat4f& vp,
                             ImVec2 image_pos, ImVec2 image_size,
                             ImVec2* out_screen) {
  const core::Vec4f clip =
      core::Vec4f(world_pos.x, world_pos.y, world_pos.z, 1.f) * vp;
  if (clip.w <= 0.f) return false;
  const float inv_w = 1.f / clip.w;
  out_screen->x =
      image_pos.x + (clip.x * inv_w * 0.5f + 0.5f) * image_size.x;
  out_screen->y =
      image_pos.y + (1.f - (clip.y * inv_w * 0.5f + 0.5f)) * image_size.y;
  return true;
}

void RoadTool::Regenerate(const EditorToolContext& ctx) const {
  if (!road_) return;
  if (ctx.terrain_data) {
    const terrain::TerrainData* td = ctx.terrain_data;
    road_->RegenerateMesh([td](float x, float z) {
      return td->GetHeight(x, z);
    });
  } else {
    road_->RegenerateMesh(nullptr);
  }
}

// ---- IsCapturingMouse --------------------------------------------------------

bool RoadTool::IsCapturingMouse() const {
  return dragging_ || hovering_;
}

// ---- RenderCreating ----------------------------------------------------------

void RoadTool::RenderCreating(const EditorToolContext& ctx,
                               ImVec2 image_pos, ImVec2 image_size) {
  const ImVec2 mouse = ImGui::GetMousePos();

  // Hint overlay (top-left of viewport).
  constexpr float kOverlayPad = 8.f;
  ImGui::SetNextWindowPos(
      ImVec2(image_pos.x + kOverlayPad, image_pos.y + kOverlayPad),
      ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.65f);
  constexpr ImGuiWindowFlags kOverlayFlags =
      ImGuiWindowFlags_NoDecoration      |
      ImGuiWindowFlags_NoInputs          |
      ImGuiWindowFlags_NoMove            |
      ImGuiWindowFlags_NoNav             |
      ImGuiWindowFlags_AlwaysAutoResize  |
      ImGuiWindowFlags_NoFocusOnAppearing;
  if (ImGui::Begin("##road_hint", nullptr, kOverlayFlags)) {
    ImGui::TextUnformatted(
        "Click to add point — Enter to finish (>= 3 pts) — Escape to cancel");
  }
  ImGui::End();

  if (!road_) {
    // No road yet: just wait for the first click.
    const bool clicked = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    if (!clicked) return;
    const auto hit = ComputeTerrainHit(ctx.camera, ctx.terrain_data,
                                        mouse, image_pos, image_size);
    if (!hit || !ctx.scene || !ctx.video) return;
    auto road = std::make_unique<game::GameRoad>(ctx.video);
    road->SetName(GenerateObjectName(*ctx.scene, "road"));
    road->GetSpline().AddControlPoint(*hit);
    road_ = static_cast<game::GameRoad*>(
        ctx.scene->AddDynamicObject(std::move(road)));
    LOG_F(INFO, "RoadTool: started road '%s' at (%.2f, %.2f, %.2f)",
          road_->GetName().c_str(), hit->x, hit->y, hit->z);
    return;
  }

  // Road exists: draw existing points and ghost line.
  const core::Camera* cam = ctx.camera->GetCamera();
  const core::Mat4f&  vp  = cam->GetViewProjectionMatrix();
  ImDrawList*         dl  = ImGui::GetWindowDrawList();

  track::RoadSpline& spline = road_->GetSpline();
  const int          count  = spline.GetPointCount();

  for (int i = 0; i < count; ++i) {
    ImVec2 sp;
    if (!ProjectPoint(spline.GetControlPoint(i), vp, image_pos, image_size, &sp))
      continue;
    dl->AddCircleFilled(sp, kPointRadius, kColDefault);
  }

  // Ghost line from last placed point to current mouse-terrain hit.
  const auto hit = ComputeTerrainHit(ctx.camera, ctx.terrain_data,
                                      mouse, image_pos, image_size);
  if (hit && count > 0) {
    ImVec2 last_sp, mouse_sp;
    if (ProjectPoint(spline.GetControlPoint(count - 1), vp, image_pos, image_size,
                     &last_sp) &&
        ProjectPoint(*hit, vp, image_pos, image_size, &mouse_sp)) {
      dl->AddLine(last_sp, mouse_sp, kColGhost, 1.5f);
      dl->AddCircle(mouse_sp, kPointRadius, kColGhost);
    }
  }

  // LMB click: append a control point.
  const bool clicked = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  if (clicked && hit) {
    spline.AddControlPoint(*hit);
    if (spline.GetPointCount() >= 2) Regenerate(ctx);
    LOG_F(9, "RoadTool: added control point at (%.2f, %.2f, %.2f)",
          hit->x, hit->y, hit->z);
  }
}

// ---- RenderEditing -----------------------------------------------------------

void RoadTool::RenderEditing(const EditorToolContext& ctx,
                              ImVec2 image_pos, ImVec2 image_size) {
  if (!road_) return;

  track::RoadSpline& spline = road_->GetSpline();
  const int          count  = spline.GetPointCount();
  ImDrawList*        dl     = ImGui::GetWindowDrawList();
  const ImVec2       mouse  = ImGui::GetMousePos();

  // Width slider overlay (top-left of viewport).
  constexpr float kOverlayPad = 8.f;
  ImGui::SetNextWindowPos(
      ImVec2(image_pos.x + kOverlayPad, image_pos.y + kOverlayPad),
      ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.65f);
  constexpr ImGuiWindowFlags kOverlayFlags =
      ImGuiWindowFlags_NoDecoration      |
      ImGuiWindowFlags_NoInputs          |
      ImGuiWindowFlags_NoMove            |
      ImGuiWindowFlags_NoNav             |
      ImGuiWindowFlags_AlwaysAutoResize  |
      ImGuiWindowFlags_NoFocusOnAppearing;
  if (ImGui::Begin("##road_overlay", nullptr, kOverlayFlags)) {
    float w = road_->GetWidth();
    ImGui::SetNextItemWidth(140.f);
    if (ImGui::SliderFloat("Road width (m)", &w, 1.f, 30.f, "%.1f")) {
      road_->SetWidth(w);
      Regenerate(ctx);
    }
  }
  ImGui::End();

  // Project control points and determine hover.
  const core::Camera* cam = ctx.camera->GetCamera();
  const core::Mat4f&  vp  = cam->GetViewProjectionMatrix();

  int   hovered_point = -1;
  float best_hover_sq = kHoverRadiusSq;

  for (int i = 0; i < count; ++i) {
    ImVec2 sp;
    if (!ProjectPoint(spline.GetControlPoint(i), vp, image_pos, image_size, &sp))
      continue;
    const float dx = mouse.x - sp.x;
    const float dy = mouse.y - sp.y;
    const float d2 = dx * dx + dy * dy;
    if (d2 < best_hover_sq) {
      best_hover_sq = d2;
      hovered_point = i;
    }
  }
  hovering_ = (hovered_point >= 0);

  // Draw circles.
  for (int i = 0; i < count; ++i) {
    ImVec2 sp;
    if (!ProjectPoint(spline.GetControlPoint(i), vp, image_pos, image_size, &sp))
      continue;
    ImU32 col = kColDefault;
    if (i == selected_point_)      col = kColSelected;
    else if (i == hovered_point)   col = kColHovered;
    dl->AddCircleFilled(sp, kPointRadius, col);
  }

  // ImGuizmo gizmo on selected point.
  if (selected_point_ >= 0 && selected_point_ < count) {
    const core::Mat4f view_t = cam->GetViewMatrix().Transpose();
    const core::Mat4f proj_t = cam->GetProjectionMatrix().Transpose();
    float view_im[16], proj_im[16];
    std::memcpy(view_im, view_t.Data(), sizeof(view_im));
    std::memcpy(proj_im, proj_t.Data(), sizeof(proj_im));

    const ImVec2 vp_pos = ImGui::GetWindowPos();
    ImGuizmo::SetRect(vp_pos.x, vp_pos.y, image_size.x, image_size.y);
    ImGuizmo::SetDrawlist(dl);
    ImGuizmo::SetOrthographic(false);

    core::Vec3f cp = spline.GetControlPoint(selected_point_);
    core::Mat4f model_t = core::Mat4f::Translation(cp).Transpose();
    float model_im[16];
    std::memcpy(model_im, model_t.Data(), sizeof(model_im));

    if (ImGuizmo::Manipulate(view_im, proj_im,
                              ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                              model_im)) {
      const core::Mat4f model_after(model_im);
      const core::Vec3f new_pos = {
          model_after(3, 0), model_after(3, 1), model_after(3, 2)};
      spline.SetControlPoint(selected_point_, new_pos);
      Regenerate(ctx);
      dragging_ = true;
    } else {
      dragging_ = ImGuizmo::IsUsing();
    }
  }

  // Click handling (LMB release, no gizmo).
  const bool clicked = ImGui::IsMouseReleased(ImGuiMouseButton_Left)
                       && !ImGuizmo::IsUsing()
                       && !ctx.gizmo_was_using;
  if (!clicked) return;

  if (hovered_point >= 0) {
    selected_point_ = hovered_point;
  } else {
    const auto hit = ComputeTerrainHit(ctx.camera, ctx.terrain_data,
                                        mouse, image_pos, image_size);
    if (hit) {
      const int insert_after = selected_point_;
      spline.InsertControlPoint(insert_after, *hit);
      selected_point_ = (insert_after < 0)
                            ? spline.GetPointCount() - 1
                            : insert_after + 1;
      Regenerate(ctx);
      LOG_F(9, "RoadTool: inserted control point at (%.2f, %.2f, %.2f)",
            hit->x, hit->y, hit->z);
    }
  }
}

// ---- OnRender ----------------------------------------------------------------

void RoadTool::OnRender(const EditorToolContext& ctx,
                         ImVec2 image_pos, ImVec2 image_size) {
  if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
    return;

  if (mode_ == Mode::kCreating) {
    RenderCreating(ctx, image_pos, image_size);
  } else {
    RenderEditing(ctx, image_pos, image_size);
  }
}

}  // namespace editor
