#include "editor/EditorCameraController.h"

#include <algorithm>
#include <cmath>

#include "core/EventType.h"
#include "core/Key.h"
#include "core/Mat4f.h"
#include "core/MouseButton.h"
#include "game/GameCamera.h"

namespace editor {

namespace {
constexpr float kOrbitSensitivity  = 0.005f;  // rad/px
constexpr float kPanSensitivity    = 0.001f;  // world-units/px per unit distance
constexpr float kZoomFactor        = 0.9f;    // distance multiplier per scroll tick
constexpr float kElevationEpsilon  = 0.01f;   // radians from ±π/2
constexpr float kHalfPi            = 1.5707963f;
constexpr float kMinDistance       = 0.1f;
constexpr float kMaxDistance       = 1000.f;
}  // namespace

EditorCameraController::EditorCameraController() = default;

void EditorCameraController::SetCamera(game::GameCamera* camera) {
  camera_ = camera;
}

void EditorCameraController::SetViewportHovered(bool hovered) {
  viewport_hovered_ = hovered;
}

void EditorCameraController::SetFocus(const core::Vec3f& focus) {
  focus_ = focus;
}

void EditorCameraController::SetDistance(float distance) {
  distance_ = std::max(kMinDistance, std::min(kMaxDistance, distance));
}

void EditorCameraController::FrameObject(const core::BBox3& bbox) {
  focus_              = bbox.GetCenter();
  const core::Vec3f s = bbox.GetSize();
  const float diag    = std::sqrt(s.x*s.x + s.y*s.y + s.z*s.z);
  distance_           = std::max(kMinDistance, std::min(kMaxDistance, diag * 1.5f));
}

void EditorCameraController::OnEvent(const core::Event& event) {
  switch (event.type) {
    case core::EventType::kKeyDown:
      if (event.key == core::Key::kLAlt || event.key == core::Key::kRAlt)
        alt_down_ = true;
      break;

    case core::EventType::kKeyUp:
      if (event.key == core::Key::kLAlt || event.key == core::Key::kRAlt)
        alt_down_ = false;
      break;

    case core::EventType::kMouseButtonDown:
      if (viewport_hovered_) {
        if (event.mouse_button == core::MouseButton::kLeft && alt_down_) {
          orbit_dragging_  = true;
          has_prev_mouse_  = false;
        }
        if (event.mouse_button == core::MouseButton::kMiddle) {
          pan_dragging_   = true;
          has_prev_mouse_ = false;
        }
      }
      break;

    case core::EventType::kMouseButtonUp:
      if (event.mouse_button == core::MouseButton::kLeft)
        orbit_dragging_ = false;
      if (event.mouse_button == core::MouseButton::kMiddle)
        pan_dragging_ = false;
      break;

    case core::EventType::kMouseMoved: {
      const float dx = has_prev_mouse_ ? (event.mouse_x - prev_mouse_x_) : 0.f;
      const float dy = has_prev_mouse_ ? (event.mouse_y - prev_mouse_y_) : 0.f;
      has_prev_mouse_ = true;
      prev_mouse_x_   = event.mouse_x;
      prev_mouse_y_   = event.mouse_y;

      if (orbit_dragging_) {
        azimuth_   += dx * kOrbitSensitivity;
        elevation_ -= dy * kOrbitSensitivity;
        elevation_  = std::max(-(kHalfPi - kElevationEpsilon),
                               std::min(kHalfPi - kElevationEpsilon, elevation_));
      }

      if (pan_dragging_) {
        const float cos_el  = std::cos(elevation_);
        const float sin_el  = std::sin(elevation_);
        const float sin_az  = std::sin(azimuth_);
        const float cos_az  = std::cos(azimuth_);
        // forward direction (from eye toward focus)
        const core::Vec3f fwd   = {-cos_el * sin_az, -sin_el, -cos_el * cos_az};
        const core::Vec3f right = core::Vec3f::kAxisY.Cross(fwd).Normalized();
        const float scale = distance_ * kPanSensitivity;
        // Drag right → scene moves right (focus translates along camera's right)
        // Drag down  → scene moves down (focus translates down by world Y)
        focus_ += right               * (dx * scale);
        focus_ -= core::Vec3f::kAxisY * (dy * scale);
      }
      break;
    }

    case core::EventType::kMouseWheelChanged:
      if (viewport_hovered_) {
        distance_ *= std::pow(kZoomFactor, event.wheel_delta);
        distance_  = std::max(kMinDistance, std::min(kMaxDistance, distance_));
      }
      break;

    default:
      break;
  }
}

void EditorCameraController::Update(float /*dt*/) {
  if (!camera_) return;

  const float cos_el = std::cos(elevation_);
  const float sin_az = std::sin(azimuth_);
  const float cos_az = std::cos(azimuth_);

  // Camera position in world space from spherical coords.
  const core::Vec3f eye = {
      focus_.x + distance_ * cos_el * sin_az,
      focus_.y + distance_ * std::sin(elevation_),
      focus_.z + distance_ * cos_el * cos_az,
  };

  const core::Vec3f forward = (focus_ - eye).Normalized();
  const core::Vec3f right   = core::Vec3f::kAxisY.Cross(forward).Normalized();

  // World transform: col0=right, col1=world-Y, col2=−forward, col3=eye.
  // GameCamera extracts position from col3 and forward from −col2.
  camera_->SetWorldTransform(core::Mat4f(
      right.x,              core::Vec3f::kAxisY.x,  -forward.x,  eye.x,
      right.y,              core::Vec3f::kAxisY.y,  -forward.y,  eye.y,
      right.z,              core::Vec3f::kAxisY.z,  -forward.z,  eye.z,
      0.f,                  0.f,                     0.f,         1.f));
}

void EditorCameraController::SetViewMatrix(const core::Mat4f& view) {
  // Extract eye position: eye = -R^T * t  (R is rows 0-2 cols 0-2, t is col 3 rows 0-2).
  const float tx = view(0, 3);
  const float ty = view(1, 3);
  const float tz = view(2, 3);
  const core::Vec3f eye = {
      -(view(0, 0) * tx + view(1, 0) * ty + view(2, 0) * tz),
      -(view(0, 1) * tx + view(1, 1) * ty + view(2, 1) * tz),
      -(view(0, 2) * tx + view(1, 2) * ty + view(2, 2) * tz),
  };

  // Recompute spherical coords relative to the unchanged focus point.
  const core::Vec3f dir = eye - focus_;
  const float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
  if (len < kMinDistance) return;

  distance_  = len;
  const core::Vec3f unit = dir * (1.f / len);
  elevation_ = std::asin(std::max(-1.f, std::min(1.f, unit.y)));
  azimuth_   = std::atan2(unit.x, unit.z);
}

}  // namespace editor
