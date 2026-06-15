#include "editor/tools/SelectionTool.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "core/BBox3.h"
#include "core/Event.h"
#include "core/Mat4f.h"
#include "core/RayUtils.h"
#include "core/Vec3f.h"
#include "core/Vec4f.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorScene.h"
#include "editor/PickingAccelerator.h"
#include "editor/commands/DeleteObjectCommand.h"
#include "editor/tools/EditorToolContext.h"
#include "game/GameCamera.h"
#include "game/GameLight.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"
#include "game/GameParticleSystem.h"
#include "game/GamePlayerStart.h"
#include "game/MeshTemplate.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/Light.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"

#include <ImGuizmo.h>
#include <imgui.h>
#include <loguru.hpp>

namespace editor {

namespace {

// Projects the 8 corners of bbox through vp and draws its 12 wireframe edges
// onto dl. image_pos / image_size define the viewport rectangle for NDC → pixel
// conversion. Edges where either endpoint falls behind the camera are skipped.
void DrawBBoxWireframe(ImDrawList* dl, const core::BBox3& bbox,
                       const core::Mat4f& vp, ImVec2 image_pos,
                       ImVec2 image_size, ImU32 color, float thickness) {
  const core::Vec3f& mn = bbox.GetMin();
  const core::Vec3f& mx = bbox.GetMax();

  const core::Vec3f corners[8] = {
    {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z},
    {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
    {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z},
    {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
  };

  struct ScreenPt { ImVec2 pos; bool valid; };
  ScreenPt sc[8];

  for (int i = 0; i < 8; ++i) {
    const core::Vec4f clip =
        core::Vec4f(corners[i].x, corners[i].y, corners[i].z, 1.f) * vp;
    if (clip.w <= 0.f) {
      sc[i].valid = false;
      continue;
    }
    const float inv_w = 1.f / clip.w;
    const float nx    = clip.x * inv_w;
    const float ny    = clip.y * inv_w;
    sc[i].pos.x = (nx + 1.f) * 0.5f * image_size.x + image_pos.x;
    sc[i].pos.y = (1.f - ny) * 0.5f * image_size.y + image_pos.y;
    sc[i].valid = true;
  }

  constexpr int kEdges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7},
  };

  for (int i = 0; i < 12; ++i) {
    if (sc[kEdges[i][0]].valid && sc[kEdges[i][1]].valid)
      dl->AddLine(sc[kEdges[i][0]].pos, sc[kEdges[i][1]].pos, color, thickness);
  }
}

// Minimum 2D distance (pixels) from screen_pt to segment [s0, s1].
float ScreenDistToSegment(ImVec2 screen_pt, ImVec2 s0, ImVec2 s1) {
  const float dx = s1.x - s0.x;
  const float dy = s1.y - s0.y;
  const float len2 = dx * dx + dy * dy;
  float t = 0.f;
  if (len2 > 1e-6f) {
    t = ((screen_pt.x - s0.x) * dx + (screen_pt.y - s0.y) * dy) / len2;
    t = std::max(0.f, std::min(1.f, t));
  }
  const float px = s0.x + t * dx - screen_pt.x;
  const float py = s0.y + t * dy - screen_pt.y;
  return std::sqrt(px * px + py * py);
}

// Projects a world-space point to screen coordinates via the VP matrix.
// Returns {pos, valid=false} if the point is behind the camera (clip.w <= 0).
struct ScreenPt { ImVec2 pos; bool valid; };

ScreenPt ProjectToScreen(const core::Vec3f& world, const core::Mat4f& vp,
                          ImVec2 image_pos, ImVec2 image_size) {
  const core::Vec4f clip =
      core::Vec4f(world.x, world.y, world.z, 1.f) * vp;
  if (clip.w <= 0.f) return {{}, false};
  const float inv_w = 1.f / clip.w;
  return {
    {
      (clip.x * inv_w + 1.f) * 0.5f * image_size.x + image_pos.x,
      (1.f - clip.y * inv_w) * 0.5f * image_size.y + image_pos.y,
    },
    true,
  };
}

// Returns the minimum screen-space distance (pixels) from cursor to the
// light's wireframe geometry. Mirrors LightWireframeRenderer's geometry.
// Returns FLT_MAX for kGlobal or if all wireframe points are behind the camera.
float LightPickDistPx(const core::Vec3f& world_pos,
                      const renderer::Light& light,
                      const core::Mat4f& vp,
                      ImVec2 image_pos, ImVec2 image_size,
                      ImVec2 cursor) {
  constexpr int kSegments = 32;
  float best = std::numeric_limits<float>::max();

  auto seg_dist = [&](const core::Vec3f& p0, const core::Vec3f& p1) {
    const ScreenPt s0 = ProjectToScreen(p0, vp, image_pos, image_size);
    const ScreenPt s1 = ProjectToScreen(p1, vp, image_pos, image_size);
    if (!s0.valid || !s1.valid) return;
    best = std::min(best, ScreenDistToSegment(cursor, s0.pos, s1.pos));
  };

  auto circle_dist = [&](const core::Vec3f& center,
                         const core::Vec3f& ax, const core::Vec3f& ay,
                         float radius) {
    for (int i = 0; i < kSegments; ++i) {
      const float a0 = static_cast<float>(i)     / kSegments * 2.f * 3.14159265f;
      const float a1 = static_cast<float>(i + 1) / kSegments * 2.f * 3.14159265f;
      seg_dist(center + ax * (std::cos(a0) * radius) + ay * (std::sin(a0) * radius),
               center + ax * (std::cos(a1) * radius) + ay * (std::sin(a1) * radius));
    }
  };

  switch (light.GetType()) {
    case renderer::LightType::kOmni: {
      const float r = static_cast<const renderer::OmniLight&>(light).GetRadius();
      circle_dist(world_pos, core::Vec3f::kAxisX, core::Vec3f::kAxisY, r);
      circle_dist(world_pos, core::Vec3f::kAxisX, core::Vec3f::kAxisZ, r);
      circle_dist(world_pos, core::Vec3f::kAxisY, core::Vec3f::kAxisZ, r);
      break;
    }
    case renderer::LightType::kCircleSpot: {
      const auto& spot  = static_cast<const renderer::CircleSpotLight&>(light);
      const float range = spot.GetRange();
      const core::Vec3f& dir = spot.GetDirection();
      const core::Vec3f up =
          (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
      const core::Vec3f right = dir.Cross(up).Normalized();
      const core::Vec3f fwd   = dir.Cross(right);
      const float base_r      = std::tan(spot.GetOuterAngle()) * range;
      const core::Vec3f base_center = world_pos + dir * range;
      seg_dist(world_pos, base_center + right * base_r);
      seg_dist(world_pos, base_center - right * base_r);
      seg_dist(world_pos, base_center + fwd   * base_r);
      seg_dist(world_pos, base_center - fwd   * base_r);
      circle_dist(base_center, right, fwd, base_r);
      break;
    }
    case renderer::LightType::kRectSpot: {
      const auto& spot  = static_cast<const renderer::RectangleSpotLight&>(light);
      const float range = spot.GetRange();
      const core::Vec3f& dir = spot.GetDirection();
      const core::Vec3f up =
          (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
      const core::Vec3f right = dir.Cross(up).Normalized();
      const core::Vec3f fwd   = dir.Cross(right);
      const float half_w = std::tan(spot.GetHAngle()) * range;
      const float half_h = std::tan(spot.GetVAngle()) * range;
      const core::Vec3f base_center = world_pos + dir * range;
      const core::Vec3f c0 = base_center + right * half_w + fwd * half_h;
      const core::Vec3f c1 = base_center - right * half_w + fwd * half_h;
      const core::Vec3f c2 = base_center - right * half_w - fwd * half_h;
      const core::Vec3f c3 = base_center + right * half_w - fwd * half_h;
      seg_dist(world_pos, c0);
      seg_dist(world_pos, c1);
      seg_dist(world_pos, c2);
      seg_dist(world_pos, c3);
      seg_dist(c0, c1);
      seg_dist(c1, c2);
      seg_dist(c2, c3);
      seg_dist(c3, c0);
      break;
    }
    case renderer::LightType::kGlobal:
      break;
  }
  return best;
}

}  // namespace

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
  if (ctx.scene->GetSelectedObject()) {
    DrawSelectedBBox(ctx, ImGui::GetWindowDrawList(), image_pos, image_size);
  }
}

void SelectionTool::PickObjectAt(const EditorToolContext& ctx,
                                  ImVec2 mouse_pos, ImVec2 image_pos,
                                  ImVec2 image_size) {
  // Normalised device coordinates: x in [-1,1], y in [-1,1] (Y flipped).
  const float ndc_x = (mouse_pos.x - image_pos.x) / image_size.x * 2.f - 1.f;
  const float ndc_y = 1.f - (mouse_pos.y - image_pos.y) / image_size.y * 2.f;

  // Unproject the NDC point on the near plane back to world space.
  const core::Camera* cam = ctx.camera->GetCamera();
  const core::Vec3f   ray_origin = cam->GetPosition();
  const core::Mat4f   vp         = cam->GetViewProjectionMatrix();
  const core::Mat4f   vp_inv     = vp.Inverse();

  const core::Vec4f clip(ndc_x, ndc_y, -1.f, 1.f);
  const core::Vec4f world4 = clip * vp_inv;
  if (std::abs(world4.w) < 1e-6f) return;
  const core::Vec3f world3(world4.x / world4.w,
                           world4.y / world4.w,
                           world4.z / world4.w);
  const core::Vec3f ray_dir = (world3 - ray_origin).Normalized();

  game::GameObject* hit    = nullptr;
  float             t_best = std::numeric_limits<float>::max();

  const std::vector<game::GameObject*>& candidates =
      ctx.picking_acc->IsBuilt()
          ? ctx.picking_acc->QueryRay(ray_origin, ray_dir)
          : ctx.scene->GetObjects();

  // Mesh pass: ray-triangle intersection.
  for (game::GameObject* obj : candidates) {
    if (obj->GetType() != game::GameObjectType::kMesh) continue;

    float t;
    if (!obj->GetWorldBBox().IntersectsRay(ray_origin, ray_dir, t)) continue;

    const auto* game_mesh = static_cast<const game::GameMesh*>(obj);
    const auto* tmpl      = game_mesh->GetTemplate();
    const auto& positions = tmpl->GetCPUPositions();

    if (positions.empty()) {
      if (t < t_best) {
        t_best = t;
        hit = obj;
      }
      continue;
    }

    // Transform the ray to model space — cheaper than transforming all vertices.
    const core::Mat4f inv_world  = obj->GetWorldTransform().Inverse();
    const core::Vec3f local_orig = core::TransformPoint(inv_world, ray_origin);
    const core::Vec3f local_dir  = core::TransformDir(inv_world, ray_dir);

    const auto& indices = tmpl->GetCPUIndices();
    for (size_t i = 0; i < indices.size(); i += 3) {
      const float tri_t = core::RayTriangleIntersect(
          local_orig, local_dir,
          positions[indices[i]], positions[indices[i + 1]], positions[indices[i + 2]]);
      if (tri_t >= 0.f && tri_t < t_best) {
        t_best = tri_t;
        hit = obj;
      }
    }
  }

  // Light pass: screen-space proximity against wireframe geometry.
  constexpr float kLightPickThresholdPx = 8.f;
  float best_light_dist_px = kLightPickThresholdPx;
  game::GameObject* best_light = nullptr;

  for (game::GameObject* obj : candidates) {
    if (obj->GetType() != game::GameObjectType::kLight) continue;
    const auto* gl    = static_cast<const game::GameLight*>(obj);
    const auto* light = gl->GetLight();
    if (!light || light->GetType() == renderer::LightType::kGlobal) continue;

    const core::Mat4f& wt = obj->GetWorldTransform();
    const core::Vec3f  pos(wt(0, 3), wt(1, 3), wt(2, 3));

    const float dist_px = LightPickDistPx(pos, *light, vp, image_pos, image_size,
                                          mouse_pos);
    if (dist_px < best_light_dist_px) {
      best_light_dist_px = dist_px;
      best_light = obj;
    }
  }

  // Prefer whichever is closer to the camera: mesh (ray-t) vs light (distance).
  if (best_light) {
    const core::Mat4f& wt = best_light->GetWorldTransform();
    const core::Vec3f  light_pos(wt(0, 3), wt(1, 3), wt(2, 3));
    const float light_depth = (light_pos - ray_origin).Length();
    if (t_best == std::numeric_limits<float>::max() || light_depth < t_best)
      hit = best_light;
  }

  // Player-start pass: screen-space proximity against base position.
  constexpr float kPlayerStartPickThresholdPx = 12.f;
  float best_ps_dist_px = kPlayerStartPickThresholdPx;
  game::GameObject* best_ps = nullptr;

  for (game::GameObject* obj : candidates) {
    if (obj->GetType() != game::GameObjectType::kPlayerStart) continue;

    const core::Mat4f& wt  = obj->GetWorldTransform();
    const core::Vec3f  pos(wt(0, 3), wt(1, 3), wt(2, 3));

    const ScreenPt sp = ProjectToScreen(pos, vp, image_pos, image_size);
    if (!sp.valid) continue;

    const float dx = sp.pos.x - mouse_pos.x;
    const float dy = sp.pos.y - mouse_pos.y;
    const float dist_px = std::sqrt(dx * dx + dy * dy);
    if (dist_px < best_ps_dist_px) {
      best_ps_dist_px = dist_px;
      best_ps = obj;
    }
  }

  if (best_ps) hit = best_ps;

  // Particle-system pass: screen-space proximity against world position.
  constexpr float kParticlePickThresholdPx = 12.f;
  float best_ps2_dist_px = kParticlePickThresholdPx;
  game::GameObject* best_particle = nullptr;

  for (game::GameObject* obj : candidates) {
    if (obj->GetType() != game::GameObjectType::kParticleSystem) continue;

    const core::Mat4f& wt  = obj->GetWorldTransform();
    const core::Vec3f  pos(wt(0, 3), wt(1, 3), wt(2, 3));

    const ScreenPt sp = ProjectToScreen(pos, vp, image_pos, image_size);
    if (!sp.valid) continue;

    const float dx = sp.pos.x - mouse_pos.x;
    const float dy = sp.pos.y - mouse_pos.y;
    const float dist_px = std::sqrt(dx * dx + dy * dy);
    if (dist_px < best_ps2_dist_px) {
      best_ps2_dist_px = dist_px;
      best_particle = obj;
    }
  }

  if (best_particle) hit = best_particle;

  ctx.scene->SetSelectedObject(hit);
}

void SelectionTool::DrawSelectedBBox(const EditorToolContext& ctx,
                                      ImDrawList* dl,
                                      ImVec2 image_pos,
                                      ImVec2 image_size) {
  DrawBBoxWireframe(dl, ctx.scene->GetSelectedObject()->GetWorldBBox(),
                    ctx.camera->GetCamera()->GetViewProjectionMatrix(),
                    image_pos, image_size,
                    IM_COL32(255, 165, 0, 255), 1.5f);
}

}  // namespace editor
