#pragma once

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/ICameraController.h"

namespace editor {

// Orbit-style camera controller for the editor viewport with fly-through mode.
//
// Controls:
//   Alt + LMB drag     — orbit (change azimuth and elevation)
//   Shift + RMB drag   — pan (translate focus point)
//   RMB (no modifier)  — fly-through: WASD to move, mouse to look
//   Scroll wheel       — dolly zooming toward the cursor position
//   Numpad 1           — snap to front view (looking in −Z)
//   Numpad 3           — snap to right view (looking in −X)
//   Numpad 7           — snap to top view (looking down)
//
// Camera state is stored as spherical coordinates relative to a focus point.
// Call SetViewportHovered() and SetViewportRect() each frame before Update()
// to gate input correctly.
//
// Lifecycle: construct → SetCamera() → per-frame: SetViewportHovered(),
//            SetViewportRect(), Update().
class EditorCameraController : public game::ICameraController {
 public:
  // Serialisable snapshot of the orbit camera state.
  struct CameraState {
    // cppcheck-suppress unusedStructMember
    core::Vec3f focus;
    // cppcheck-suppress unusedStructMember
    float       azimuth;
    // cppcheck-suppress unusedStructMember
    float       elevation;
    // cppcheck-suppress unusedStructMember
    float       distance;
  };

  EditorCameraController();

  // ICameraController interface.
  void SetCamera(game::GameCamera* camera) override;
  void OnEvent(const core::Event& event) override;
  void Update(float dt) override;

  // Gate mouse input: only orbit/pan/zoom when the viewport panel is hovered.
  // EditorViewport must call this once per frame before Update().
  void SetViewportHovered(bool hovered);

  // Provides the viewport panel screen rect for zoom-toward-cursor unprojection.
  // EditorViewport must call this once per frame before Update().
  void SetViewportRect(float x, float y, float w, float h);

  // Teleport the orbit focus to a new world-space point.
  void SetFocus(const core::Vec3f& focus);

  // Set the orbit radius (clamped to [0.1, 1000]).
  void SetDistance(float distance);

  // Returns the current orbit radius.
  [[nodiscard]] float GetDistance() const { return distance_; }

  // Adjusts focus and distance so that bbox fits in the view.
  void FrameObject(const core::BBox3& bbox);

  // Snaps the camera orientation to match the given row-major view matrix
  // while keeping the current focus point. Called by EditorViewport when
  // ImGuizmo::ViewManipulate returns a modified matrix.
  void SetViewMatrix(const core::Mat4f& view);

  // Returns a snapshot of the current orbit state for save/restore.
  [[nodiscard]] CameraState GetState() const;

  // Teleports the camera to the given state; immediately recomputes the matrix.
  void SetState(const CameraState& state);

 private:
  // Transitions from fly-through mode back to orbit, updating azimuth_,
  // elevation_, and focus_ from the current fly state.
  void ExitFlyMode();

  // cppcheck-suppress unusedStructMember
  game::GameCamera* camera_ = nullptr;

  // Spherical coordinates relative to focus_.
  // cppcheck-suppress unusedStructMember
  core::Vec3f focus_     = core::Vec3f::kZero;
  // cppcheck-suppress unusedStructMember
  float       azimuth_   = 0.f;
  // cppcheck-suppress unusedStructMember
  float       elevation_ = 0.3f;  // ~17 degrees
  // cppcheck-suppress unusedStructMember
  float       distance_  = 15.f;

  // Modifier key state.
  // cppcheck-suppress unusedStructMember
  bool  alt_down_   = false;
  // cppcheck-suppress unusedStructMember
  bool  shift_down_ = false;

  // Drag / fly mode state.
  // cppcheck-suppress unusedStructMember
  bool  viewport_hovered_ = false;
  // cppcheck-suppress unusedStructMember
  bool  orbit_dragging_   = false;
  // cppcheck-suppress unusedStructMember
  bool  pan_dragging_     = false;
  // cppcheck-suppress unusedStructMember
  bool  fly_active_       = false;
  // cppcheck-suppress unusedStructMember
  bool  has_prev_mouse_   = false;
  // cppcheck-suppress unusedStructMember
  float prev_mouse_x_     = 0.f;
  // cppcheck-suppress unusedStructMember
  float prev_mouse_y_     = 0.f;

  // Fly-through state (active while RMB is held without Shift).
  // cppcheck-suppress unusedStructMember
  core::Vec3f fly_pos_   = core::Vec3f::kZero;
  // cppcheck-suppress unusedStructMember
  float       fly_yaw_   = 0.f;
  // cppcheck-suppress unusedStructMember
  float       fly_pitch_ = 0.f;
  // WASD movement keys (tracked while fly_active_).
  // cppcheck-suppress unusedStructMember
  bool w_down_ = false;
  // cppcheck-suppress unusedStructMember
  bool a_down_ = false;
  // cppcheck-suppress unusedStructMember
  bool s_down_ = false;
  // cppcheck-suppress unusedStructMember
  bool d_down_ = false;

  // Viewport panel screen rect, updated each frame by SetViewportRect().
  // Used by the scroll handler for zoom-toward-cursor unprojection.
  // cppcheck-suppress unusedStructMember
  float vp_x_ = 0.f;
  // cppcheck-suppress unusedStructMember
  float vp_y_ = 0.f;
  // cppcheck-suppress unusedStructMember
  float vp_w_ = 1.f;
  // cppcheck-suppress unusedStructMember
  float vp_h_ = 1.f;
  // Latest cursor position in window space (updated every MouseMoved event).
  // cppcheck-suppress unusedStructMember
  float curr_mouse_x_ = 0.f;
  // cppcheck-suppress unusedStructMember
  float curr_mouse_y_ = 0.f;
};

}  // namespace editor
