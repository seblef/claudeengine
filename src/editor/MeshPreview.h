#pragma once

#include <memory>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/CoordinateSystem.h"
#include "core/ProjectionType.h"
#include "core/Vec3f.h"

namespace game  { class MeshTemplate; }
namespace renderer {
class GlobalLight;
class MeshInstance;
class PreviewRenderer;
}

namespace editor {

// Renders a game::MeshTemplate into an offscreen FBO and displays it as an
// ImGui::Image with orbit-camera mouse controls.
//
// Mouse controls (active when the image is hovered):
//   Left drag  — orbit: azimuth (horizontal) and elevation (vertical, clamped
//                to ±89°).
//   Scroll wheel — zoom: clamps distance to [0.5, 5] × bbox_diagonal.
//
// Usage:
//   MeshPreview preview(video, 256, 256);
//   preview.SetTemplate(tmpl);          // once, or when template changes
//   // In the ImGui frame:
//   preview.Render(time);               // renders and calls ImGui::Image()
class MeshPreview {
 public:
  MeshPreview(abstract::VideoDevice* video, int width, int height);
  ~MeshPreview();

  // Replaces the displayed template; resets orbit distance to 1.5 × bbox diagonal
  // and centers the camera on the mesh bounding box. Pass nullptr to clear.
  void SetTemplate(game::MeshTemplate* tmpl);

  // Renders the preview scene to the internal FBO then calls ImGui::Image().
  // Mouse controls are updated when the image is hovered.
  void Render(float time);

 private:
  void UpdateCamera();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  int                    w_;
  // cppcheck-suppress unusedStructMember
  int                    h_;

  // Orbit camera state.
  float azimuth_deg_   = 45.f;
  float elevation_deg_ = 30.f;
  float distance_      = 3.f;
  float bbox_diag_     = 1.f;
  // cppcheck-suppress unusedStructMember
  core::Vec3f center_{0.f, 0.f, 0.f};

  // Preview camera (perspective, right-handed).
  core::Camera camera_;

  // Offscreen color render target and FBO (depth is inside PreviewRenderer).
  std::unique_ptr<abstract::RenderTarget>      color_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> rtg_;

  std::unique_ptr<renderer::PreviewRenderer> preview_renderer_;

  // Preview light — directional, warm sun-like colour.
  std::unique_ptr<renderer::GlobalLight> light_;

  // Current template and its MeshInstance.
  // cppcheck-suppress unusedStructMember
  game::MeshTemplate*                     tmpl_     = nullptr;
  std::unique_ptr<renderer::MeshInstance> instance_;
};

}  // namespace editor
