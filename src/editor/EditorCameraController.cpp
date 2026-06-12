#include "editor/EditorCameraController.h"

#include <algorithm>
#include <cmath>

#include "core/Camera.h"
#include "core/EventType.h"
#include "core/Key.h"
#include "core/Mat4f.h"
#include "core/MouseButton.h"
#include "core/Vec4f.h"
#include "game/GameCamera.h"

namespace editor {

namespace {
constexpr float kOrbitSensitivity  = 0.005f;   // rad/px
constexpr float kPanSensitivity    = 0.001f;   // world-units/px per unit distance
constexpr float kZoomFactor        = 0.9f;     // distance multiplier per scroll tick
constexpr float kElevationEpsilon  = 0.01f;    // radians from ±π/2
constexpr float kHalfPi            = 1.5707963f;
constexpr float kMinDistance       = 0.1f;
constexpr float kMaxDistance       = 1000.f;
// Fly-through movement speed as a fraction of the current orbit distance per second.
// At distance=15 the camera moves at 15 world-units/sec; at distance=100 at 100 u/s.
constexpr float kFlySpeedFactor    = 1.0f;
}  // namespace

EditorCameraController::EditorCameraController() = default;

void EditorCameraController::SetCamera(game::GameCamera* camera) {
  camera_ = camera;
}

void EditorCameraController::SetViewportHovered(bool hovered) {
  viewport_hovered_ = hovered;
}

void EditorCameraController::SetViewportRect(float x, float y, float w, float h) {
  vp_x_ = x; vp_y_ = y; vp_w_ = w; vp_h_ = h;
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

void EditorCameraController::ExitFlyMode() {
  if (!fly_active_) return;
  azimuth_   = fly_yaw_;
  elevation_ = std::max(-(kHalfPi - kElevationEpsilon),
                         std::min(kHalfPi - kElevationEpsilon, fly_pitch_));
  // Place orbit focus in front of the fly camera at the current distance.
  const float cos_p = std::cos(fly_pitch_);
  const core::Vec3f fwd = {
      -cos_p * std::sin(fly_yaw_),
      -std::sin(fly_pitch_),
      -cos_p * std::cos(fly_yaw_),
  };
  focus_     = fly_pos_ + fwd * distance_;
  fly_active_ = false;
  w_down_ = a_down_ = s_down_ = d_down_ = e_down_ = q_down_ = false;
}

void EditorCameraController::OnEvent(const core::Event& event) {
  switch (event.type) {
    // ---- Modifier / movement key tracking ------------------------------------

    case core::EventType::kKeyDown:
      switch (event.key) {
        case core::Key::kLAlt:
        case core::Key::kRAlt:    alt_down_   = true;  break;
        case core::Key::kLShift:
        case core::Key::kRShift:  shift_down_ = true;  break;
        case core::Key::kLCtrl:
        case core::Key::kRCtrl:   ctrl_down_  = true;  break;
        case core::Key::kW:       w_down_     = true;  break;
        case core::Key::kA:       a_down_     = true;  break;
        case core::Key::kS:       s_down_     = true;  break;
        case core::Key::kD:       d_down_     = true;  break;
        case core::Key::kE:       e_down_     = true;  break;
        case core::Key::kQ:       q_down_     = true;  break;

        // Preset views (Blender numpad conventions).
        case core::Key::kNumpad1:
          if (viewport_hovered_ && !fly_active_) {
            elevation_ = 0.f;
            azimuth_   = 0.f;
          }
          break;
        case core::Key::kNumpad3:
          if (viewport_hovered_ && !fly_active_) {
            elevation_ = 0.f;
            azimuth_   = kHalfPi;
          }
          break;
        case core::Key::kNumpad7:
          if (viewport_hovered_ && !fly_active_) {
            elevation_ = kHalfPi - kElevationEpsilon;
            azimuth_   = 0.f;
          }
          break;
        default: break;
      }
      break;

    case core::EventType::kKeyUp:
      switch (event.key) {
        case core::Key::kLAlt:
        case core::Key::kRAlt:    alt_down_   = false; break;
        case core::Key::kLShift:
        case core::Key::kRShift:  shift_down_ = false; break;
        case core::Key::kLCtrl:
        case core::Key::kRCtrl:   ctrl_down_  = false; break;
        case core::Key::kW:       w_down_     = false; break;
        case core::Key::kA:       a_down_     = false; break;
        case core::Key::kS:       s_down_     = false; break;
        case core::Key::kD:       d_down_     = false; break;
        case core::Key::kE:       e_down_     = false; break;
        case core::Key::kQ:       q_down_     = false; break;
        default: break;
      }
      break;

    // Release all drag/fly state when focus leaves the window.
    case core::EventType::kWindowLostFocus:
      orbit_dragging_ = false;
      pan_dragging_   = false;
      alt_down_       = false;
      shift_down_     = false;
      ctrl_down_      = false;
      ExitFlyMode();
      break;

    // ---- Mouse button events -------------------------------------------------

    case core::EventType::kMouseButtonDown:
      if (viewport_hovered_) {
        if (event.mouse_button == core::MouseButton::kLeft && alt_down_) {
          orbit_dragging_ = true;
          has_prev_mouse_ = false;
        }
        if (event.mouse_button == core::MouseButton::kRight) {
          has_prev_mouse_ = false;
          if (shift_down_) {
            // Shift+RMB → pan.
            pan_dragging_ = true;
          } else {
            // RMB alone → enter fly-through mode.
            // Initialise fly state from the current orbit position.
            const float cos_el = std::cos(elevation_);
            const float sin_az = std::sin(azimuth_);
            const float cos_az = std::cos(azimuth_);
            fly_pos_ = {
                focus_.x + distance_ * cos_el * sin_az,
                focus_.y + distance_ * std::sin(elevation_),
                focus_.z + distance_ * cos_el * cos_az,
            };
            fly_yaw_    = azimuth_;
            fly_pitch_  = elevation_;
            fly_active_ = true;
          }
        }
      }
      break;

    case core::EventType::kMouseButtonUp:
      if (event.mouse_button == core::MouseButton::kLeft)
        orbit_dragging_ = false;
      if (event.mouse_button == core::MouseButton::kRight) {
        pan_dragging_ = false;
        ExitFlyMode();
      }
      break;

    // ---- Mouse movement ------------------------------------------------------

    case core::EventType::kMouseMoved: {
      curr_mouse_x_ = event.mouse_x;
      curr_mouse_y_ = event.mouse_y;

      const float dx = has_prev_mouse_ ? (event.mouse_x - prev_mouse_x_) : 0.f;
      const float dy = has_prev_mouse_ ? (event.mouse_y - prev_mouse_y_) : 0.f;
      has_prev_mouse_ = true;
      prev_mouse_x_   = event.mouse_x;
      prev_mouse_y_   = event.mouse_y;

      if (orbit_dragging_) {
        azimuth_   += dx * kOrbitSensitivity;
        // Positive dy (drag down) → higher elevation (camera goes up, looks down).
        // Positive drag up (dy < 0) → lower elevation (looks up). Non-inverted.
        elevation_ += dy * kOrbitSensitivity;
        elevation_  = std::max(-(kHalfPi - kElevationEpsilon),
                               std::min(kHalfPi - kElevationEpsilon, elevation_));
      }

      if (pan_dragging_) {
        const float cos_el  = std::cos(elevation_);
        const float sin_el  = std::sin(elevation_);
        const float sin_az  = std::sin(azimuth_);
        const float cos_az  = std::cos(azimuth_);
        const core::Vec3f fwd   = {-cos_el * sin_az, -sin_el, -cos_el * cos_az};
        const core::Vec3f right = core::Vec3f::kAxisY.Cross(fwd).Normalized();
        const float scale = distance_ * kPanSensitivity;
        focus_ += right               * (dx * scale);
        focus_ -= core::Vec3f::kAxisY * (dy * scale);
      }

      if (fly_active_) {
        // Drag right (dx > 0) → turn right → yaw decreases (our convention).
        fly_yaw_   -= dx * kOrbitSensitivity;
        // Drag down (dy > 0) → look down → pitch increases. Consistent with orbit.
        fly_pitch_ += dy * kOrbitSensitivity;
        fly_pitch_  = std::max(-(kHalfPi - kElevationEpsilon),
                               std::min(kHalfPi - kElevationEpsilon, fly_pitch_));
      }
      break;
    }

    // ---- Scroll: zoom toward cursor ------------------------------------------

    case core::EventType::kMouseWheelChanged:
      if (viewport_hovered_ && !fly_active_) {
        const float old_dist = distance_;
        distance_ *= std::pow(kZoomFactor, event.wheel_delta);
        distance_  = std::max(kMinDistance, std::min(kMaxDistance, distance_));

        // Shift focus toward the world point under the cursor so zooming
        // keeps the hovered region roughly stationary on screen.
        if (camera_ && vp_w_ > 0.f && vp_h_ > 0.f) {
          const float ndc_x = (curr_mouse_x_ - vp_x_) / vp_w_ * 2.f - 1.f;
          const float ndc_y = 1.f - (curr_mouse_y_ - vp_y_) / vp_h_ * 2.f;

          const core::Camera* cam = camera_->GetCamera();
          const core::Vec3f   eye = cam->GetPosition();
          const core::Mat4f   vp_inv = cam->GetViewProjectionMatrix().Inverse();

          const core::Vec4f clip(ndc_x, ndc_y, -1.f, 1.f);
          const core::Vec4f world4 = clip * vp_inv;
          if (std::abs(world4.w) > 1e-6f) {
            const core::Vec3f world3(world4.x / world4.w,
                                     world4.y / world4.w,
                                     world4.z / world4.w);
            const core::Vec3f ray_dir = (world3 - eye).Normalized();
            // World point on a sphere of radius old_dist centred at the eye.
            const core::Vec3f cursor_world = eye + ray_dir * old_dist;
            // Move focus proportionally toward cursor_world as distance shrinks.
            const float shift_t = 1.f - (distance_ / old_dist);
            focus_ += (cursor_world - focus_) * shift_t;
          }
        }
      }
      break;

    default:
      break;
  }
}

void EditorCameraController::Update(float dt) {
  if (!camera_) return;

  if (fly_active_) {
    // Fly-through: derive camera orientation from fly_yaw_ / fly_pitch_.
    const float cos_p = std::cos(fly_pitch_);
    const float sin_p = std::sin(fly_pitch_);
    const float sin_y = std::sin(fly_yaw_);
    const float cos_y = std::cos(fly_yaw_);
    // Forward uses the same sign convention as orbit (−Z when yaw=pitch=0).
    const core::Vec3f fwd   = {-cos_p * sin_y, -sin_p, -cos_p * cos_y};
    const core::Vec3f right = core::Vec3f::kAxisY.Cross(fwd).Normalized();

    // WASD/QE movement — suppressed while Ctrl is held so that Ctrl+shortcut
    // key combos (e.g. Ctrl+Z for undo) do not accidentally move the camera.
    if (!ctrl_down_) {
      const float speed = distance_ * kFlySpeedFactor * dt;
      if (w_down_) fly_pos_ += fwd                  * speed;
      if (s_down_) fly_pos_ -= fwd                  * speed;
      if (a_down_) fly_pos_ -= right                * speed;
      if (d_down_) fly_pos_ += right                * speed;
      if (e_down_) fly_pos_ += core::Vec3f::kAxisY  * speed;
      if (q_down_) fly_pos_ -= core::Vec3f::kAxisY  * speed;
    }

    camera_->SetWorldTransform(core::Mat4f(
        right.x, core::Vec3f::kAxisY.x, -fwd.x, fly_pos_.x,
        right.y, core::Vec3f::kAxisY.y, -fwd.y, fly_pos_.y,
        right.z, core::Vec3f::kAxisY.z, -fwd.z, fly_pos_.z,
        0.f,     0.f,                    0.f,    1.f));
    return;
  }

  // Orbit mode.
  const float cos_el = std::cos(elevation_);
  const float sin_az = std::sin(azimuth_);
  const float cos_az = std::cos(azimuth_);

  // W/S translate the focus forward/backward along the look direction so the
  // user can reposition the orbit pivot without entering fly mode.
  // Suppressed while Ctrl is held to avoid conflicts with Ctrl+key shortcuts.
  if (!ctrl_down_ && (w_down_ || s_down_)) {
    const core::Vec3f fwd = {-cos_el * sin_az, -std::sin(elevation_), -cos_el * cos_az};
    const float speed = distance_ * kFlySpeedFactor * dt;
    if (w_down_) focus_ += fwd * speed;
    if (s_down_) focus_ -= fwd * speed;
  }

  const core::Vec3f eye = {
      focus_.x + distance_ * cos_el * sin_az,
      focus_.y + distance_ * std::sin(elevation_),
      focus_.z + distance_ * cos_el * cos_az,
  };

  const core::Vec3f forward = (focus_ - eye).Normalized();
  const core::Vec3f right   = core::Vec3f::kAxisY.Cross(forward).Normalized();

  camera_->SetWorldTransform(core::Mat4f(
      right.x, core::Vec3f::kAxisY.x, -forward.x, eye.x,
      right.y, core::Vec3f::kAxisY.y, -forward.y, eye.y,
      right.z, core::Vec3f::kAxisY.z, -forward.z, eye.z,
      0.f,     0.f,                    0.f,        1.f));
}

EditorCameraController::CameraState EditorCameraController::GetState() const {
  return {focus_, azimuth_, elevation_, distance_};
}

void EditorCameraController::SetState(const CameraState& state) {
  focus_     = state.focus;
  azimuth_   = state.azimuth;
  elevation_ = state.elevation;
  distance_  = std::max(kMinDistance, std::min(kMaxDistance, state.distance));
  Update(0.f);
}

void EditorCameraController::SetViewMatrix(const core::Mat4f& view) {
  const float tx = view(0, 3);
  const float ty = view(1, 3);
  const float tz = view(2, 3);
  const core::Vec3f eye = {
      -(view(0, 0) * tx + view(1, 0) * ty + view(2, 0) * tz),
      -(view(0, 1) * tx + view(1, 1) * ty + view(2, 1) * tz),
      -(view(0, 2) * tx + view(1, 2) * ty + view(2, 2) * tz),
  };

  const core::Vec3f dir = eye - focus_;
  const float len = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
  if (len < kMinDistance) return;

  distance_  = len;
  const core::Vec3f unit = dir * (1.f / len);
  elevation_ = std::asin(std::max(-1.f, std::min(1.f, unit.y)));
  azimuth_   = std::atan2(unit.x, unit.z);
}

}  // namespace editor
