#include "editor/EditorViewport.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>

#include "abstract/TextureFormat.h"
#include "core/CoordinateSystem.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"
#include "core/Vec4f.h"
#include "editor/EditorScene.h"
#include "editor/EditorTool.h"
#include "editor/commands/DeleteObjectCommand.h"
#include "editor/commands/PlaceObjectCommand.h"
#include "editor/commands/TransformCommand.h"
#include "game/GameLight.h"
#include "game/GameMesh.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"
#include "game/MeshTemplate.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GlobalLight.h"
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

// Projects a 3D line segment p0→p1 and draws it on dl.
// Skips segments where either endpoint is behind the camera (clip.w <= 0).
void DrawLineSegment3D(ImDrawList* dl,
                       const core::Vec3f& p0, const core::Vec3f& p1,
                       const core::Mat4f& vp,
                       ImVec2 image_pos, ImVec2 image_size,
                       ImU32 color, float thickness) {
  const core::Vec4f c0 =
      core::Vec4f(p0.x, p0.y, p0.z, 1.f) * vp;
  const core::Vec4f c1 =
      core::Vec4f(p1.x, p1.y, p1.z, 1.f) * vp;

  if (c0.w <= 0.f || c1.w <= 0.f) return;

  const float inv0 = 1.f / c0.w;
  const float inv1 = 1.f / c1.w;

  ImVec2 s0, s1;
  s0.x = (c0.x * inv0 + 1.f) * 0.5f * image_size.x + image_pos.x;
  s0.y = (1.f - c0.y * inv0) * 0.5f * image_size.y + image_pos.y;
  s1.x = (c1.x * inv1 + 1.f) * 0.5f * image_size.x + image_pos.x;
  s1.y = (1.f - c1.y * inv1) * 0.5f * image_size.y + image_pos.y;

  dl->AddLine(s0, s1, color, thickness);
}

// Approximates a circle as a 32-segment polyline and draws it on dl.
// center: world-space center; ax/ay: two orthogonal world-space axis vectors
// spanning the circle plane; radius: circle radius.
void DrawCircle3D(ImDrawList* dl,
                  const core::Vec3f& center,
                  const core::Vec3f& ax, const core::Vec3f& ay,
                  float radius,
                  const core::Mat4f& vp,
                  ImVec2 image_pos, ImVec2 image_size,
                  ImU32 color, float thickness) {
  constexpr int kSegments = 32;
  for (int i = 0; i < kSegments; ++i) {
    const float a0 = static_cast<float>(i)     / kSegments * 2.f * 3.14159265f;
    const float a1 = static_cast<float>(i + 1) / kSegments * 2.f * 3.14159265f;
    const core::Vec3f p0 = center + ax * (std::cos(a0) * radius)
                                  + ay * (std::sin(a0) * radius);
    const core::Vec3f p1 = center + ax * (std::cos(a1) * radius)
                                  + ay * (std::sin(a1) * radius);
    DrawLineSegment3D(dl, p0, p1, vp, image_pos, image_size, color, thickness);
  }
}

// Draws a wireframe representation of a light onto dl.
// world_pos: light position; light: the renderer light; vp: camera VP matrix.
// is_selected: true ↔ use orange highlight colour, else yellow.
void DrawLightWireframe(ImDrawList* dl,
                        const core::Vec3f& world_pos,
                        const renderer::Light& light,
                        const core::Mat4f& vp,
                        ImVec2 image_pos, ImVec2 image_size,
                        bool is_selected) {
  constexpr ImU32 kColorUnselected = 0xFFFFCC00u;  // yellow, ABGR
  constexpr ImU32 kColorSelected   = 0xFF0088FFu;  // orange, ABGR
  constexpr float kThickness       = 1.5f;

  const ImU32 color = is_selected ? kColorSelected : kColorUnselected;

  switch (light.GetType()) {
    case renderer::LightType::kOmni: {
      const auto& omni = static_cast<const renderer::OmniLight&>(light);
      const float r    = omni.GetRadius();
      DrawCircle3D(dl, world_pos,
                   core::Vec3f::kAxisX, core::Vec3f::kAxisY, r,
                   vp, image_pos, image_size, color, kThickness);
      DrawCircle3D(dl, world_pos,
                   core::Vec3f::kAxisX, core::Vec3f::kAxisZ, r,
                   vp, image_pos, image_size, color, kThickness);
      DrawCircle3D(dl, world_pos,
                   core::Vec3f::kAxisY, core::Vec3f::kAxisZ, r,
                   vp, image_pos, image_size, color, kThickness);
      break;
    }

    case renderer::LightType::kCircleSpot: {
      const auto& spot = static_cast<const renderer::CircleSpotLight&>(light);
      const float range = spot.GetRange();
      const core::Vec3f& dir = spot.GetDirection();

      // Build two orthogonal axes in the base plane.
      const core::Vec3f up =
          (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
      const core::Vec3f right = dir.Cross(up).Normalized();
      const core::Vec3f fwd   = dir.Cross(right);

      const float base_r = std::tan(spot.GetOuterAngle()) * range;
      const core::Vec3f base_center = world_pos + dir * range;

      // 4 lines from apex to rim at 0°/90°/180°/270°.
      DrawLineSegment3D(dl, world_pos, base_center + right * base_r,
                        vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, world_pos, base_center - right * base_r,
                        vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, world_pos, base_center + fwd * base_r,
                        vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, world_pos, base_center - fwd * base_r,
                        vp, image_pos, image_size, color, kThickness);

      // Base circle.
      DrawCircle3D(dl, base_center, right, fwd, base_r,
                   vp, image_pos, image_size, color, kThickness);
      break;
    }

    case renderer::LightType::kRectSpot: {
      const auto& spot = static_cast<const renderer::RectangleSpotLight&>(light);
      const float range = spot.GetRange();
      const core::Vec3f& dir = spot.GetDirection();

      const core::Vec3f up =
          (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
      const core::Vec3f right = dir.Cross(up).Normalized();
      const core::Vec3f fwd   = dir.Cross(right);

      const float half_w = std::tan(spot.GetHAngle()) * range;
      const float half_h = std::tan(spot.GetVAngle()) * range;
      const core::Vec3f base_center = world_pos + dir * range;

      // 4 corners of the base rectangle.
      const core::Vec3f c0 = base_center + right * half_w + fwd * half_h;
      const core::Vec3f c1 = base_center - right * half_w + fwd * half_h;
      const core::Vec3f c2 = base_center - right * half_w - fwd * half_h;
      const core::Vec3f c3 = base_center + right * half_w - fwd * half_h;

      // 4 lines from apex to corners.
      DrawLineSegment3D(dl, world_pos, c0, vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, world_pos, c1, vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, world_pos, c2, vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, world_pos, c3, vp, image_pos, image_size, color, kThickness);

      // Base rectangle perimeter.
      DrawLineSegment3D(dl, c0, c1, vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, c1, c2, vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, c2, c3, vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, c3, c0, vp, image_pos, image_size, color, kThickness);
      break;
    }

    case renderer::LightType::kGlobal: {
      const auto& global = static_cast<const renderer::GlobalLight&>(light);
      const core::Vec3f& d = global.GetDirection();
      constexpr float kLen = 5.f;

      // Two arrow lines from world origin in the light direction.
      const core::Vec3f tip = d * kLen;
      DrawLineSegment3D(dl, core::Vec3f::kZero, tip,
                        vp, image_pos, image_size, color, kThickness);
      // Arrowhead: small orthogonal lines at the tip.
      const core::Vec3f perp =
          (std::abs(d.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
      const core::Vec3f side = d.Cross(perp).Normalized() * (kLen * 0.15f);
      DrawLineSegment3D(dl, tip, tip - d * (kLen * 0.2f) + side,
                        vp, image_pos, image_size, color, kThickness);
      DrawLineSegment3D(dl, tip, tip - d * (kLen * 0.2f) - side,
                        vp, image_pos, image_size, color, kThickness);
      break;
    }
  }
}

// Möller-Trumbore ray-triangle intersection.
// Returns t >= 0 on hit, -1.f on miss.
float RayTriangleIntersect(const core::Vec3f& orig, const core::Vec3f& dir,
                           const core::Vec3f& v0,   const core::Vec3f& v1,
                           const core::Vec3f& v2) {
  constexpr float kEpsilon = 1e-7f;
  const core::Vec3f e1 = v1 - v0;
  const core::Vec3f e2 = v2 - v0;
  const core::Vec3f h  = dir.Cross(e2);
  const float a = e1.Dot(h);
  if (std::abs(a) < kEpsilon) return -1.f;
  const float f = 1.f / a;
  const core::Vec3f s = orig - v0;
  const float u = f * s.Dot(h);
  if (u < 0.f || u > 1.f) return -1.f;
  const core::Vec3f q = s.Cross(e1);
  const float v = f * dir.Dot(q);
  if (v < 0.f || u + v > 1.f) return -1.f;
  const float t = f * e2.Dot(q);
  return t >= 0.f ? t : -1.f;
}

// Transform a world-space point to model space.
core::Vec3f TransformPoint(const core::Mat4f& inv_world, const core::Vec3f& p) {
  return p * inv_world;
}

// Transform a world-space direction to model space (no translation).
core::Vec3f TransformDir(const core::Mat4f& inv_world, const core::Vec3f& d) {
  return core::TransformNoTranslation(d, inv_world);
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
      camera_ctrl_(std::make_unique<EditorCameraController>()) {
  camera_->SetFOV(0.785398f);  // 45 degrees
  camera_->SetMinDepth(0.1f);
  camera_->SetMaxDepth(1000.f);
  camera_ctrl_->SetCamera(camera_.get());
}

void EditorViewport::OnEvent(const core::Event& event) {
  if (event.type == core::EventType::kKeyDown &&
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

void EditorViewport::ResizeIfNeeded(int w, int h) {
  if (w == static_cast<int>(panel_size_.x) && h == static_cast<int>(panel_size_.y)) return;
  panel_size_ = {static_cast<float>(w), static_cast<float>(h)};
  camera_->SetScreenCenter({panel_size_.x * 0.5f, panel_size_.y * 0.5f});
  renderer::Renderer::Instance().ResizeTargets(w, h);
  render_target_ = video_->CreateRenderTarget(w, h, abstract::TextureFormat::kRGBA8);
  abstract::RenderTarget* color_ptr = render_target_.get();
  render_fbo_ = video_->CreateRenderTargetGroup(
      std::span<abstract::RenderTarget*>(&color_ptr, 1), nullptr);
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

  // Object picking: LMB release without Alt (Alt+LMB is camera orbit).
  // Also skipped when a gizmo is being dragged or hovered.
  if (scene_ && selection_active_ &&
      ImGui::IsWindowHovered() && !ImGuizmo::IsViewManipulateHovered() &&
      !ImGuizmo::IsOver() &&
      !ImGui::GetIO().KeyAlt && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    PickObjectAt(ImGui::GetMousePos(), image_pos, avail);
  }

  // Bounding box wireframe for the selected object.
  if (scene_ && scene_->GetSelectedObject()) {
    DrawSelectedBBox(ImGui::GetWindowDrawList(), image_pos, avail);
  }

  // Light wireframe overlays.
  if (scene_) {
    DrawLightsOverlay(ImGui::GetWindowDrawList(), image_pos, avail);
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

    if (gizmo_was_using_ && !gizmo_using && history_) {
      const core::Mat4f after = selected->GetWorldTransform();
      if (after != gizmo_before_transform_)
        history_->Push(std::make_unique<TransformCommand>(
            selected, gizmo_before_transform_, after));
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
  const core::Mat4f   vp_inv     = cam->GetViewProjectionMatrix().Inverse();

  const core::Vec4f clip(ndc_x, ndc_y, -1.f, 1.f);
  const core::Vec4f world4 = clip * vp_inv;
  if (std::abs(world4.w) < 1e-6f) return;
  const core::Vec3f world3(world4.x / world4.w,
                           world4.y / world4.w,
                           world4.z / world4.w);
  const core::Vec3f ray_dir = (world3 - ray_origin).Normalized();

  game::GameObject* hit    = nullptr;
  float             t_best = std::numeric_limits<float>::max();

  for (game::GameObject* obj : scene_->GetObjects()) {
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
    const core::Vec3f local_orig = TransformPoint(inv_world, ray_origin);
    const core::Vec3f local_dir  = TransformDir(inv_world, ray_dir);

    const auto& indices = tmpl->GetCPUIndices();
    for (size_t i = 0; i < indices.size(); i += 3) {
      const float tri_t = RayTriangleIntersect(
          local_orig, local_dir,
          positions[indices[i]], positions[indices[i + 1]], positions[indices[i + 2]]);
      if (tri_t >= 0.f && tri_t < t_best) {
        t_best = tri_t;
        hit = obj;
      }
    }
  }

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
  const float ndc_x = (mouse_pos.x - image_pos.x) / image_size.x * 2.f - 1.f;
  const float ndc_y = 1.f - (mouse_pos.y - image_pos.y) / image_size.y * 2.f;

  const core::Camera* cam        = camera_->GetCamera();
  const core::Vec3f   ray_origin = cam->GetPosition();
  const core::Mat4f   vp_inv     = cam->GetViewProjectionMatrix().Inverse();

  const core::Vec4f clip(ndc_x, ndc_y, -1.f, 1.f);
  const core::Vec4f world4 = clip * vp_inv;
  if (std::abs(world4.w) < 1e-6f) return;
  const core::Vec3f world3(world4.x / world4.w,
                           world4.y / world4.w,
                           world4.z / world4.w);
  const core::Vec3f ray_dir = (world3 - ray_origin).Normalized();

  if (std::abs(ray_dir.y) < 1e-4f) return;  // nearly parallel to floor
  const float t = -ray_origin.y / ray_dir.y;
  if (t < 0.f) return;  // behind the camera

  const core::Vec3f hit = ray_origin + ray_dir * t;

  if (pending_preview_)
    preview_object_ = scene_->AddDynamicObject(std::move(pending_preview_));

  if (preview_object_)
    preview_object_->SetWorldTransform(
        core::Mat4f::Translation({hit.x, preview_height_, hit.z}));
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

  const float ndc_x = (mouse_pos.x - image_pos.x) / image_size.x * 2.f - 1.f;
  const float ndc_y = 1.f - (mouse_pos.y - image_pos.y) / image_size.y * 2.f;

  const core::Camera* cam        = camera_->GetCamera();
  const core::Vec3f   ray_origin = cam->GetPosition();
  const core::Mat4f   vp_inv     = cam->GetViewProjectionMatrix().Inverse();

  const core::Vec4f clip(ndc_x, ndc_y, -1.f, 1.f);
  const core::Vec4f world4 = clip * vp_inv;
  if (std::abs(world4.w) < 1e-6f) return;
  const core::Vec3f world3(world4.x / world4.w,
                           world4.y / world4.w,
                           world4.z / world4.w);
  const core::Vec3f ray_dir = (world3 - ray_origin).Normalized();

  if (std::abs(ray_dir.y) < 1e-4f) return;  // nearly parallel to floor
  const float t = -ray_origin.y / ray_dir.y;
  if (t < 0.f) return;  // behind the camera

  const core::Vec3f hit = ray_origin + ray_dir * t;

  auto mesh = std::make_unique<game::GameMesh>(tmpl);
  mesh->SetWorldTransform(core::Mat4f::Translation({hit.x, 0.f, hit.z}));

  if (history_) {
    history_->Push(std::make_unique<PlaceObjectCommand>(scene_, std::move(mesh)));
  } else {
    game::GameObject* obj = scene_->AddDynamicObject(std::move(mesh));
    scene_->SetSelectedObject(obj);
  }
}


void EditorViewport::DrawLightsOverlay(ImDrawList* dl, ImVec2 image_pos,
                                        ImVec2 image_size) const {
  const core::Mat4f vp = camera_->GetCamera()->GetViewProjectionMatrix();
  const game::GameObject* selected = scene_->GetSelectedObject();

  for (const game::GameObject* obj : scene_->GetObjects()) {
    if (obj->GetType() != game::GameObjectType::kLight) continue;

    const auto* game_light = static_cast<const game::GameLight*>(obj);
    const renderer::Light* light = game_light->GetLight();
    if (!light) continue;

    // Extract world position from the world transform (translation is in col 3).
    const core::Mat4f& wt = obj->GetWorldTransform();
    const core::Vec3f world_pos(wt(0, 3), wt(1, 3), wt(2, 3));

    const bool is_selected = (obj == selected);
    DrawLightWireframe(dl, world_pos, *light, vp, image_pos, image_size,
                       is_selected);
  }
}

EditorCameraController::CameraState EditorViewport::GetCameraState() const {
  return camera_ctrl_->GetState();
}

void EditorViewport::SetCameraState(const EditorCameraController::CameraState& state) {
  camera_ctrl_->SetState(state);
}

}  // namespace editor
