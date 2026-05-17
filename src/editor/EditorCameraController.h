#pragma once

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "game/ICameraController.h"

namespace editor {

// Orbit-style camera controller for the editor viewport.
//
// Controls:
//   Alt + LMB drag — orbit (change azimuth and elevation)
//   MMB drag       — pan (translate focus point)
//   Scroll wheel   — dolly (change orbit distance)
//
// Camera state is stored as spherical coordinates relative to a focus point.
// Call SetViewportHovered() each frame before Update() to gate input so
// ImGui UI interactions do not affect the camera.
//
// Lifecycle: construct → SetCamera() → SetViewportHovered() each frame → Update().
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

  // Input state.
  // cppcheck-suppress unusedStructMember
  bool  viewport_hovered_ = false;
  // cppcheck-suppress unusedStructMember
  bool  alt_down_         = false;
  // cppcheck-suppress unusedStructMember
  bool  orbit_dragging_   = false;
  // cppcheck-suppress unusedStructMember
  bool  pan_dragging_     = false;
  // cppcheck-suppress unusedStructMember
  bool  has_prev_mouse_   = false;
  // cppcheck-suppress unusedStructMember
  float prev_mouse_x_     = 0.f;
  // cppcheck-suppress unusedStructMember
  float prev_mouse_y_     = 0.f;
};

}  // namespace editor
