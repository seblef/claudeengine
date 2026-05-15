#include "editor/EditorViewport.h"

#include <algorithm>
#include <cstring>
#include <span>

#include "abstract/TextureFormat.h"
#include "core/CoordinateSystem.h"
#include "core/Mat4f.h"
#include "core/ProjectionType.h"
#include "renderer/Renderer.h"

#include <ImGuizmo.h>
#include <imgui.h>

namespace editor {

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

  // ImGuizmo must be initialised once per frame before any other ImGuizmo call.
  ImGuizmo::SetOrthographic(false);
  ImGuizmo::BeginFrame();

  // Gate orbit/pan/zoom input: block when the ViewManipulate widget is hovered.
  const bool widget_hovered = ImGuizmo::IsViewManipulateHovered();
  camera_ctrl_->SetViewportHovered(ImGui::IsWindowHovered() && !widget_hovered);
  camera_ctrl_->Update(ImGui::GetIO().DeltaTime);

  ResizeIfNeeded(w, h);

  renderer::Renderer::Instance().Update(
      static_cast<float>(ImGui::GetTime()),
      camera_->GetCamera(),
      render_fbo_.get());

  if (render_target_) {
    // uv0=(0,1) uv1=(1,0): Y-flip because OpenGL FBO origin is bottom-left.
    ImGui::Image(render_target_->GetNativeHandle(), avail, ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));
  }

  // XYZ axis overlay — bottom-right corner of the viewport panel.
  // ImGuizmo expects a column-major float[16] view matrix, while our Mat4f
  // is row-major, so we pass the transpose and transpose the result back.
  const ImVec2 vp_pos  = ImGui::GetWindowPos();
  const ImVec2 vp_size = {static_cast<float>(w), static_cast<float>(h)};
  ImGuizmo::SetRect(vp_pos.x, vp_pos.y, vp_size.x, vp_size.y);

  // Build a column-major copy from the row-major view matrix.
  // ImGuizmo modifies the float* in-place, so we need a writable buffer.
  const core::Mat4f view_t = camera_->GetCamera()->GetViewMatrix().Transpose();
  float view_cm[16];
  std::memcpy(view_cm, view_t.Data(), sizeof(view_cm));

  constexpr float kWidgetSize = 88.f;
  const ImVec2 widget_pos = {
      vp_pos.x + vp_size.x - kWidgetSize,
      vp_pos.y + vp_size.y - kWidgetSize,
  };
  ImGuizmo::ViewManipulate(view_cm, camera_ctrl_->GetDistance(),
                           widget_pos, ImVec2(kWidgetSize, kWidgetSize),
                           0x10101080);

  // If ViewManipulate changed the matrix the user clicked an axis face;
  // convert back to row-major and update the controller orientation.
  const core::Mat4f view_t_after(view_cm);
  if (view_t_after != view_t) {
    camera_ctrl_->SetViewMatrix(view_t_after.Transpose());
  }
}

}  // namespace editor
