#include "editor/MeshPreview.h"

#include <algorithm>
#include <cmath>
#include <span>

#include <imgui.h>

#include "abstract/TextureFormat.h"
#include "core/BBox3.h"
#include "core/Color.h"
#include "core/Mat4f.h"
#include "game/MeshTemplate.h"
#include "renderer/GlobalLight.h"
#include "renderer/MeshInstance.h"
#include "renderer/PreviewRenderer.h"

namespace editor {

namespace {
constexpr float kOrbitSensitivity = 0.4f;
constexpr float kZoomStep         = 0.3f;
constexpr float kDegToRad         = 3.14159265f / 180.f;
}  // namespace

MeshPreview::MeshPreview(abstract::VideoDevice* video, int width, int height)
    : video_(video),
      w_(width),
      h_(height),
      camera_(core::ProjectionType::kPerspective,
              core::CoordinateSystem::kRightHanded),
      preview_renderer_(std::make_unique<renderer::PreviewRenderer>(video, w_, h_)),
      light_(std::make_unique<renderer::GlobalLight>(
          core::Color(0.9f, 0.85f, 0.7f), 1.0f,
          core::Vec3f(0.05f, 0.05f, 0.05f),
          core::Vec3f(-1.f, -2.f, -1.f).Normalized())) {
  camera_.SetFOV(0.785398f);  // 45 degrees
  camera_.SetMinDepth(0.1f);
  camera_.SetMaxDepth(1000.f);
  camera_.SetScreenCenter(
      {static_cast<float>(w_) * 0.5f, static_cast<float>(h_) * 0.5f});

  color_rt_ = video_->CreateRenderTarget(w_, h_, abstract::TextureFormat::kRGBA8);
  abstract::RenderTarget* color_ptr = color_rt_.get();
  rtg_ = video_->CreateRenderTargetGroup(
      std::span<abstract::RenderTarget*>(&color_ptr, 1), nullptr);
}

MeshPreview::~MeshPreview() = default;

void MeshPreview::SetTemplate(game::MeshTemplate* tmpl) {
  tmpl_     = tmpl;
  instance_.reset();
  if (!tmpl_) return;

  const core::BBox3& bbox = tmpl_->GetLocalBBox();
  bbox_diag_ = bbox.GetSize().Length();
  center_    = bbox.GetCenter();
  distance_  = 1.5f * (bbox_diag_ > 1e-4f ? bbox_diag_ : 1.f);

  instance_ = std::make_unique<renderer::MeshInstance>(
      tmpl_->GetMesh(), core::Mat4f::kIdentity, /*always_visible=*/true);
}

void MeshPreview::Render(float time) {
  if (instance_) {
    UpdateCamera();
    preview_renderer_->Render(time, camera_, instance_.get(), light_.get(),
                               rtg_.get());
  }

  const ImVec2 size(static_cast<float>(w_), static_cast<float>(h_));

  // InvisibleButton captures mouse input (including LMB drag), preventing the
  // parent window from being moved when the user orbits the camera.
  const ImVec2 pos = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##preview", size);

  // Draw the render target over the button area with a Y-flip (OpenGL FBO
  // origin is bottom-left).
  ImGui::GetWindowDrawList()->AddImage(
      color_rt_->GetNativeHandle(),
      pos, ImVec2(pos.x + size.x, pos.y + size.y),
      ImVec2(0.f, 1.f), ImVec2(1.f, 0.f));

  if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
    const ImGuiIO& io = ImGui::GetIO();

    if (io.MouseDown[ImGuiMouseButton_Left]) {
      azimuth_deg_   += io.MouseDelta.x * kOrbitSensitivity;
      elevation_deg_ -= io.MouseDelta.y * kOrbitSensitivity;
      elevation_deg_  = std::clamp(elevation_deg_, -89.f, 89.f);
    }

    const float scroll = io.MouseWheel;
    if (scroll != 0.f) {
      distance_ -= scroll * kZoomStep * bbox_diag_;
      distance_  = std::clamp(distance_,
                               0.5f * bbox_diag_, 5.f * bbox_diag_);
    }
  }
}

void MeshPreview::UpdateCamera() {
  const float azi_rad = azimuth_deg_   * kDegToRad;
  const float ele_rad = elevation_deg_ * kDegToRad;
  const float cos_e   = std::cos(ele_rad);

  const core::Vec3f offset{
    distance_ * cos_e * std::sin(azi_rad),
    distance_ * std::sin(ele_rad),
    distance_ * cos_e * std::cos(azi_rad),
  };

  camera_.SetPosition(center_ + offset);
  camera_.SetTarget(center_);
  camera_.SetUp(core::Vec3f::kAxisY);
  camera_.UpdateMatrices();
}

}  // namespace editor
