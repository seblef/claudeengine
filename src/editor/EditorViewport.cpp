#include "editor/EditorViewport.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "abstract/TextureFormat.h"
#include "core/CoordinateSystem.h"
#include "terrain/TerrainData.h"
#include "core/Mat4f.h"
#include "core/RayUtils.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"
#include "core/Vec4f.h"
#include "editor/EditorScene.h"
#include "editor/EditorTool.h"
#include "editor/LightWireframeRenderer.h"
#include "editor/PickingAccelerator.h"
#include "editor/commands/DeleteObjectCommand.h"
#include "editor/commands/PlaceObjectCommand.h"
#include "editor/commands/TransformCommand.h"
#include "game/GameLight.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"
#include "game/GameParticleSystem.h"
#include "game/GamePlayerStart.h"
#include "game/MeshTemplate.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/Light.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"
#include "renderer/Renderer.h"

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

// Returns the ImGuizmo operation for a tool, or nullopt if no gizmo should
// be shown (kSelection and kCamera do not render a transform gizmo).
std::optional<ImGuizmo::OPERATION> ToolToImGuizmoOp(EditorTool tool) {
  switch (tool) {
    case EditorTool::kTranslate: return ImGuizmo::TRANSLATE;
    case EditorTool::kRotate:    return ImGuizmo::ROTATE;
    case EditorTool::kScale:     return ImGuizmo::SCALE;
    default:                     return std::nullopt;
  }
}


}  // namespace

EditorViewport::EditorViewport(abstract::VideoDevice* video)
    : video_(video),
      camera_(std::make_unique<game::GameCamera>(
          core::ProjectionType::kPerspective,
          core::CoordinateSystem::kRightHanded)),
      camera_ctrl_(std::make_unique<EditorCameraController>()),
      light_wireframe_(video),
      player_start_gizmo_(video),
      particle_gizmo_(video) {
  camera_->SetFOV(0.785398f);  // 45 degrees
  camera_->SetMinDepth(0.1f);
  camera_->SetMaxDepth(1000.f);
  camera_ctrl_->SetCamera(camera_.get());
}

void EditorViewport::SetScene(EditorScene* scene) {
  scene_ = scene;
  if (scene_) {
    scene_->SetOnObjectAdded([this](game::GameObject* obj) {
      picking_acc_.Add(obj);
    });
    scene_->SetOnObjectRemoved([this](game::GameObject* obj) {
      picking_acc_.Remove(obj);
    });
    picking_acc_.Build(scene_->GetObjects(), scene_->GetBounds());
  }
}

void EditorViewport::OnEvent(const core::Event& event) {
  const bool keyboard_captured = ImGui::GetIO().WantCaptureKeyboard;

  // Delete selected object — suppressed when ImGui has keyboard focus (e.g. a
  // text input widget) so that deleting text does not also delete scene objects.
  if (!keyboard_captured &&
      event.type == core::EventType::kKeyDown &&
      event.key == core::Key::kDelete &&
      !ImGuizmo::IsUsing() &&
      scene_ != nullptr) {
    game::GameObject* selected = scene_->GetSelectedObject();
    if (selected && scene_->IsDynamic(selected)) {
      LOG_F(INFO, "Deleting object '%s'", selected->GetName().c_str());
      history_->Push(
          std::make_unique<DeleteObjectCommand>(scene_, selected));
      selected_object_ = nullptr;
    }
  }

  // Always forward key-up events to the camera to clear any held movement
  // flags, but suppress key-down events when ImGui owns the keyboard so that
  // typing in a text field does not accidentally move the camera.
  if (keyboard_captured && event.type == core::EventType::kKeyDown)
    return;

  camera_ctrl_->OnEvent(event);
}

void EditorViewport::BeginPreview(std::unique_ptr<game::GameObject> obj,
                                   float height, ImGuiMouseCursor cursor) {
  pending_preview_ = std::move(obj);
  preview_height_  = height;
  preview_cursor_  = cursor;
  preview_active_  = true;
  SetSelectionActive(false);
}

void EditorViewport::CancelPreview() {
  if (preview_object_ && scene_) {
    scene_->RemoveDynamicObject(preview_object_);
    preview_object_ = nullptr;
  }
  pending_preview_.reset();
  preview_active_ = false;
  SetSelectionActive(true);
}

void EditorViewport::SetPendingMeshTemplate(game::MeshTemplate* tmpl) {
  if (tmpl)
    BeginPreview(std::make_unique<game::GameMesh>(tmpl), 0.f, ImGuiMouseCursor_None);
  else
    CancelPreview();
}

void EditorViewport::SetPendingLightType(std::optional<renderer::LightType> type) {
  if (type.has_value())
    BeginPreview(std::make_unique<game::GameLight>(*type), 10.f,
                 ImGuiMouseCursor_ResizeAll);
  else
    CancelPreview();
}

void EditorViewport::SetPendingPlayerStart() {
  BeginPreview(std::make_unique<game::GamePlayerStart>(), 0.f,
               ImGuiMouseCursor_ResizeAll);
}

void EditorViewport::SetPendingParticleSystem(
    const std::string& template_name) {
  particles::ParticleSystemTemplate* tmpl =
      particles::ParticleSystemTemplate::GetOrLoad(template_name, video_);
  if (!tmpl) return;
  auto ps = std::make_unique<game::GameParticleSystem>(tmpl, video_);
  tmpl->Release();  // GameParticleSystem holds the AddRef'd reference
  BeginPreview(std::move(ps), 0.f, ImGuiMouseCursor_ResizeAll);
}

void EditorViewport::ResizeIfNeeded(int w, int h) {
  if (w == static_cast<int>(panel_size_.x) && h == static_cast<int>(panel_size_.y)) return;
  panel_size_ = {static_cast<float>(w), static_cast<float>(h)};
  camera_->SetScreenCenter({panel_size_.x * 0.5f, panel_size_.y * 0.5f});
  renderer::Renderer::Instance().ResizeTargets(w, h);
  render_target_ = video_->CreateRenderTarget(w, h, abstract::TextureFormat::kRGBA8);
  abstract::RenderTarget* color_ptr = render_target_.get();
  render_fbo_ = video_->CreateRenderTargetGroup(
      std::span<abstract::RenderTarget*>(&color_ptr, 1), nullptr);
  // Share the GBuffer depth so light wireframes are occluded by opaque geometry.
  abstract::RenderTarget* depth_ptr =
      renderer::Renderer::Instance().GetGBuffer()->GetDepthRT();
  wireframe_fbo_ = video_->CreateRenderTargetGroup(
      std::span<abstract::RenderTarget*>(&color_ptr, 1), depth_ptr);
}

void EditorViewport::Render() {
  const ImVec2 avail = ImGui::GetContentRegionAvail();
  const int w = std::max(1, static_cast<int>(avail.x));
  const int h = std::max(1, static_cast<int>(avail.y));

  // Gate orbit/pan/zoom input: block when the ViewManipulate widget is hovered.
  const bool widget_hovered = ImGuizmo::IsViewManipulateHovered();
  camera_ctrl_->SetViewportHovered(ImGui::IsWindowHovered() && !widget_hovered);
  camera_ctrl_->Update(ImGui::GetIO().DeltaTime);

  ResizeIfNeeded(w, h);

  renderer::Renderer::Instance().Update(
      static_cast<float>(ImGui::GetTime()),
      camera_->GetCamera(),
      render_fbo_.get());

  // Capture the image top-left before the Image widget advances the cursor.
  const ImVec2 image_pos = ImGui::GetCursorScreenPos();
  camera_ctrl_->SetViewportRect(image_pos.x, image_pos.y, avail.x, avail.y);

  if (render_target_) {
    // uv0=(0,1) uv1=(1,0): Y-flip because OpenGL FBO origin is bottom-left.
    ImGui::Image(render_target_->GetNativeHandle(), avail, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
    if (scene_ && ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("MESH_TEMPLATE")) {
        auto* tmpl = *static_cast<game::MeshTemplate**>(p->Data);
        PlaceMeshAt(ImGui::GetMousePos(), image_pos, avail, tmpl);
      }
      ImGui::EndDragDropTarget();
    }
  }

  // Placement mode: preview follows the cursor; LMB click confirms placement.
  if (scene_ && preview_active_ && ImGui::IsWindowHovered()) {
    ImGui::SetMouseCursor(preview_cursor_);
    UpdatePreviewPosition(ImGui::GetMousePos(), image_pos, avail);
    if (!ImGui::GetIO().KeyAlt && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
      PlacePreview();
  }

  // Sculpt brush: LMB drag when sculpt mode is active.
  // Overrides object picking for the duration of the stroke.
  if (sculpt_active_ && scene_ && ImGui::IsWindowHovered()) {
    const bool key_alt   = ImGui::GetIO().KeyAlt;
    const bool lmb_down  = ImGui::IsMouseDown(ImGuiMouseButton_Left) && !key_alt;

    if (lmb_down) {
      const auto hit = ComputeTerrainHit(ImGui::GetMousePos(), image_pos, avail);
      if (hit && on_sculpt_brush_) {
        const bool first = !sculpt_stroke_active_;
        sculpt_stroke_active_ = true;
        on_sculpt_brush_(hit->x, hit->z, first, ImGui::GetIO().DeltaTime);
      }
    } else if (sculpt_stroke_active_) {
      sculpt_stroke_active_ = false;
      if (on_sculpt_end_) on_sculpt_end_();
    }
  }

  // Object picking: LMB release without Alt (Alt+LMB is camera orbit).
  // Skipped when the gizmo is hovered (IsOver) or was being dragged last frame
  // (gizmo_was_using_): the drag-end release must not trigger a spurious pick
  // because the gizmo code that clears gizmo_was_using_ runs after this block.
  if (scene_ && selection_active_ && !sculpt_active_ &&
      ImGui::IsWindowHovered() && !ImGuizmo::IsViewManipulateHovered() &&
      !ImGuizmo::IsOver() && !gizmo_was_using_ &&
      !ImGui::GetIO().KeyAlt && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    PickObjectAt(ImGui::GetMousePos(), image_pos, avail);
  }

  // Bounding box wireframe for the selected object.
  if (scene_ && scene_->GetSelectedObject()) {
    DrawSelectedBBox(ImGui::GetWindowDrawList(), image_pos, avail);
  }

  // Light wireframes rendered into the scene FBO with depth testing so lights
  // behind opaque geometry are correctly occluded.
  if (scene_ && wireframe_fbo_) {
    light_wireframe_.Render(scene_->GetObjects(), scene_->GetSelectedObject(),
                            wireframe_fbo_.get());
  }

  // Player-start flag gizmos rendered without depth testing (always visible).
  if (scene_ && render_fbo_) {
    player_start_gizmo_.Render(scene_->GetObjects(), scene_->GetSelectedObject(),
                               render_fbo_.get());
  }

  // Particle system sphere gizmos rendered without depth testing.
  if (scene_ && render_fbo_) {
    particle_gizmo_.Render(scene_->GetObjects(), scene_->GetSelectedObject(),
                           render_fbo_.get());
  }

  // Terrain wireframe debug overlay — flat white edges over the composited scene.
  if (terrain_wireframe_debug_ && wireframe_fbo_) {
    renderer::Renderer::Instance().RenderTerrainWireframe(
        *camera_->GetCamera(), wireframe_fbo_.get());
  }

  // XYZ axis overlay — bottom-right corner of the viewport panel.
  const ImVec2 vp_pos  = ImGui::GetWindowPos();
  const ImVec2 vp_size = {static_cast<float>(w), static_cast<float>(h)};
  ImGuizmo::SetRect(vp_pos.x, vp_pos.y, vp_size.x, vp_size.y);

  // ImGuizmo uses row-major storage with row-vector convention (translation in
  // last row), which is the transpose of our column-vector Mat4f convention.
  const core::Mat4f view_t = camera_->GetCamera()->GetViewMatrix().Transpose();
  const core::Mat4f proj_t = camera_->GetCamera()->GetProjectionMatrix().Transpose();
  float view_im[16], proj_im[16];
  std::memcpy(view_im, view_t.Data(), sizeof(view_im));
  std::memcpy(proj_im, proj_t.Data(), sizeof(proj_im));

  // Route drawing to the current viewport window's draw list, not the
  // background overlay created by BeginFrame().
  ImGuizmo::SetDrawlist();

  // Transform gizmo for the selected object.
  game::GameObject* selected = scene_ ? scene_->GetSelectedObject() : nullptr;
  const auto gizmo_op = ToolToImGuizmoOp(active_tool_);
  if (selected && gizmo_op) {
    const core::Mat4f model_t = selected->GetWorldTransform().Transpose();
    float model_im[16];
    std::memcpy(model_im, model_t.Data(), sizeof(model_im));

    ImGuizmo::Manipulate(view_im, proj_im, *gizmo_op, ImGuizmo::WORLD, model_im);

    const bool gizmo_using = ImGuizmo::IsUsing();

    if (!gizmo_was_using_ && gizmo_using)
      gizmo_before_transform_ = selected->GetWorldTransform();

    if (gizmo_using) {
      const core::Mat4f model_t_after(model_im);
      selected->SetWorldTransform(model_t_after.Transpose());
    }

    if (gizmo_was_using_ && !gizmo_using) {
      picking_acc_.UpdateMoved(selected);
      if (history_) {
        const core::Mat4f after = selected->GetWorldTransform();
        if (after != gizmo_before_transform_)
          history_->Push(std::make_unique<TransformCommand>(
              selected, gizmo_before_transform_, after));
      }
    }

    gizmo_was_using_ = gizmo_using;
  } else {
    gizmo_was_using_ = false;
  }

  // Use the 9-parameter overload so ComputeContext runs first and sets
  // gContext.mProjectionMat. Without it the 5-parameter form reads a zero
  // projection matrix, concludes left-handed, and negates all angles.
  float dummy_model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  constexpr float kWidgetSize = 88.f;
  const ImVec2 widget_pos = {
      vp_pos.x + vp_size.x - kWidgetSize,
      vp_pos.y + vp_size.y - kWidgetSize,
  };
  ImGuizmo::ViewManipulate(view_im, proj_im, ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
                           dummy_model, camera_ctrl_->GetDistance(),
                           widget_pos, ImVec2(kWidgetSize, kWidgetSize), 0x10101080);

  // ImGuizmo wrote back its row-vector view matrix; transpose to our
  // column-vector convention before updating the camera controller.
  const core::Mat4f view_t_after(view_im);
  if (view_t_after != view_t) {
    camera_ctrl_->SetViewMatrix(view_t_after.Transpose());
  }
}

void EditorViewport::PickObjectAt(ImVec2 mouse_pos, ImVec2 image_pos,
                                   ImVec2 image_size) {
  // Normalised device coordinates: x in [-1,1], y in [-1,1] (Y flipped).
  const float ndc_x = (mouse_pos.x - image_pos.x) / image_size.x * 2.f - 1.f;
  const float ndc_y = 1.f - (mouse_pos.y - image_pos.y) / image_size.y * 2.f;

  // Unproject the NDC point on the near plane back to world space.
  const core::Camera* cam = camera_->GetCamera();
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
      picking_acc_.IsBuilt()
          ? picking_acc_.QueryRay(ray_origin, ray_dir)
          : scene_->GetObjects();

  // Mesh pass: ray-triangle intersection.
  for (game::GameObject* obj : candidates) {
    if (obj->GetType() != game::GameObjectType::kMesh) continue;

    // Level 2: bbox pre-filter.
    float t;
    if (!obj->GetWorldBBox().IntersectsRay(ray_origin, ray_dir, t)) continue;

    const auto* game_mesh = static_cast<const game::GameMesh*>(obj);
    const auto* tmpl      = game_mesh->GetTemplate();
    const auto& positions = tmpl->GetCPUPositions();

    if (positions.empty()) {
      // Procedural mesh: fall back to bbox hit distance.
      if (t < t_best) {
        t_best = t;
        hit = obj;
      }
      continue;
    }

    // Level 3: ray-triangle in model space.
    // Transforming the ray is cheaper than transforming all vertices.
    // Note: t values are in model space; for uniform-scale transforms this
    // equals world-space t. For non-uniform scale the comparison is approximate
    // but sufficient for editor picking.
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

  // Light pass: screen-space proximity picking against the wireframe geometry.
  // A light is a candidate if the cursor is within kLightPickThresholdPx pixels
  // of any wireframe segment. The closest light (by camera distance) that passes
  // the threshold wins, provided it is not occluded by a closer mesh hit.
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

  // Prefer whichever is closer to the camera: mesh hit (by ray-t) vs light hit
  // (by camera-to-light-center distance). This rejects lights that are fully
  // behind a closer opaque mesh.
  if (best_light) {
    const core::Mat4f& wt = best_light->GetWorldTransform();
    const core::Vec3f  light_pos(wt(0, 3), wt(1, 3), wt(2, 3));
    const float light_depth = (light_pos - ray_origin).Length();
    if (t_best == std::numeric_limits<float>::max() || light_depth < t_best)
      hit = best_light;
  }

  // Player-start pass: screen-space proximity picking against the base position.
  // Since the gizmo is not depth-tested, player starts are always selectable.
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

  if (best_ps)
    hit = best_ps;

  // Particle-system pass: screen-space proximity picking against world position.
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

  if (best_particle)
    hit = best_particle;

  scene_->SetSelectedObject(hit);
}

void EditorViewport::DrawSelectedBBox(ImDrawList* dl, ImVec2 image_pos,
                                       ImVec2 image_size) const {
  DrawBBoxWireframe(dl, scene_->GetSelectedObject()->GetWorldBBox(),
                    camera_->GetCamera()->GetViewProjectionMatrix(),
                    image_pos, image_size,
                    IM_COL32(255, 165, 0, 255), 1.5f);
}

void EditorViewport::UpdatePreviewPosition(ImVec2 mouse_pos, ImVec2 image_pos,
                                            ImVec2 image_size) {
  const auto hit = ComputeTerrainHit(mouse_pos, image_pos, image_size);
  if (!hit) return;

  if (pending_preview_)
    preview_object_ = scene_->AddDynamicObject(std::move(pending_preview_));

  if (preview_object_) {
    preview_object_->SetWorldTransform(
        core::Mat4f::Translation({hit->x, hit->y + preview_height_, hit->z}));
    picking_acc_.UpdateMoved(preview_object_);
  }
}

void EditorViewport::PlacePreview() {
  if (!preview_object_) return;  // no valid floor hit yet

  if (history_) {
    auto obj = scene_->ReclaimDynamicObject(preview_object_);
    preview_object_ = nullptr;
    history_->Push(std::make_unique<PlaceObjectCommand>(scene_, std::move(obj)));
  } else {
    scene_->SetSelectedObject(preview_object_);
    preview_object_ = nullptr;
  }

  preview_active_ = false;
  SetSelectionActive(true);

  if (on_placement_done_) on_placement_done_();
}

void EditorViewport::PlaceMeshAt(ImVec2 mouse_pos, ImVec2 image_pos,
                                  ImVec2 image_size, game::MeshTemplate* tmpl) {
  if (!scene_ || !tmpl) return;

  const auto hit = ComputeTerrainHit(mouse_pos, image_pos, image_size);
  if (!hit) return;

  auto mesh = std::make_unique<game::GameMesh>(tmpl);
  mesh->SetWorldTransform(core::Mat4f::Translation(*hit));

  if (history_) {
    history_->Push(std::make_unique<PlaceObjectCommand>(scene_, std::move(mesh)));
  } else {
    game::GameObject* obj = scene_->AddDynamicObject(std::move(mesh));
    scene_->SetSelectedObject(obj);
  }
}



std::optional<core::Vec3f> EditorViewport::ComputeTerrainHit(
    ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size) const {
  const float ndc_x = (mouse_pos.x - image_pos.x) / image_size.x * 2.f - 1.f;
  const float ndc_y = 1.f - (mouse_pos.y - image_pos.y) / image_size.y * 2.f;

  const core::Camera* cam        = camera_->GetCamera();
  const core::Vec3f   ray_origin = cam->GetPosition();
  const core::Mat4f   vp_inv     = cam->GetViewProjectionMatrix().Inverse();

  const core::Vec4f clip(ndc_x, ndc_y, -1.f, 1.f);
  const core::Vec4f world4 = clip * vp_inv;
  if (std::abs(world4.w) < 1e-6f) return std::nullopt;
  const core::Vec3f world3(world4.x / world4.w,
                           world4.y / world4.w,
                           world4.z / world4.w);
  const core::Vec3f ray_dir = (world3 - ray_origin).Normalized();

  // When no terrain data is available, fall back to the y=0 horizontal plane.
  if (!terrain_data_) {
    if (std::abs(ray_dir.y) < 1e-4f) return std::nullopt;
    const float t = -ray_origin.y / ray_dir.y;
    if (t < 0.f) return std::nullopt;
    return ray_origin + ray_dir * t;
  }

  // Ray-march against the heightmap with binary-search refinement.
  constexpr int   kSteps    = 64;
  constexpr float kMaxDist  = 2000.f;
  const float     step      = kMaxDist / kSteps;

  float prev_t = 0.f;
  for (int i = 0; i < kSteps; ++i) {
    const float  t = static_cast<float>(i + 1) * step;
    const core::Vec3f p = ray_origin + ray_dir * t;
    const float terrain_h = terrain_data_->GetHeight(p.x, p.z);
    if (p.y <= terrain_h) {
      // Binary search between prev_t and t.
      float lo = prev_t;
      float hi = t;
      for (int j = 0; j < 8; ++j) {
        const float mid   = (lo + hi) * 0.5f;
        const core::Vec3f mp   = ray_origin + ray_dir * mid;
        if (mp.y <= terrain_data_->GetHeight(mp.x, mp.z))
          hi = mid;
        else
          lo = mid;
      }
      return ray_origin + ray_dir * ((lo + hi) * 0.5f);
    }
    prev_t = t;
  }
  return std::nullopt;
}

EditorCameraController::CameraState EditorViewport::GetCameraState() const {
  return camera_ctrl_->GetState();
}

void EditorViewport::SetCameraState(const EditorCameraController::CameraState& state) {
  camera_ctrl_->SetState(state);
}

void EditorViewport::FrameObject(const core::BBox3& bbox) {
  camera_ctrl_->FrameObject(bbox);
}

void EditorViewport::SetActiveTool(EditorToolBase* tool) {
  if (active_tool_base_)
    active_tool_base_->OnDeactivate();
  active_tool_base_ = tool;
  if (active_tool_base_) {
    const EditorToolContext ctx{scene_, camera_.get(), &picking_acc_,
                                history_, video_};
    active_tool_base_->OnActivate(ctx);
  }
}

}  // namespace editor
