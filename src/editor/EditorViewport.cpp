#include "editor/EditorViewport.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>

#include "abstract/TextureFormat.h"
#include "core/CoordinateSystem.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "core/Vec4f.h"
#include "editor/EditorScene.h"
#include "game/GameObject.h"
#include "game/GameObjectType.h"
#include "renderer/Renderer.h"

#include <ImGuizmo.h>
#include <imgui.h>

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
  camera_ctrl_->OnEvent(event);
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
  }

  // Object picking: LMB release without Alt (Alt+LMB is camera orbit).
  if (scene_ && selection_active_ &&
      ImGui::IsWindowHovered() && !ImGuizmo::IsViewManipulateHovered() &&
      !ImGui::GetIO().KeyAlt && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
    PickObjectAt(ImGui::GetMousePos(), image_pos, avail);
  }

  // Bounding box wireframe for the selected object.
  if (scene_ && scene_->GetSelectedObject()) {
    DrawSelectedBBox(ImGui::GetWindowDrawList(), image_pos, avail);
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

  // Ray-AABB test against each mesh object; lights are skipped (infinite bbox).
  game::GameObject* hit    = nullptr;
  float             t_best = std::numeric_limits<float>::max();

  for (game::GameObject* obj : scene_->GetObjects()) {
    if (obj->GetType() == game::GameObjectType::kLight) continue;
    float t;
    if (obj->GetWorldBBox().IntersectsRay(ray_origin, ray_dir, t) && t < t_best) {
      t_best = t;
      hit    = obj;
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

}  // namespace editor
