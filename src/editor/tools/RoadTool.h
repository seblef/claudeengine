#pragma once

#include "core/Event.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "editor/tools/EditorToolBase.h"

#include <imgui.h>

namespace game { class GameRoad; }

namespace editor {

class EditorScene;

// Tool for in-scene spline editing of a GameRoad object.
//
// Operates in two modes:
//   kEditing  — user selects and drags existing control points; entered via
//               OnActivate() when a GameRoad is already selected.
//   kCreating — user clicks terrain to lay control points one by one; entered
//               via OnActivateCreation() when the Create Road toolbar action
//               fires.  Enter finalises (≥ 3 points required); Escape cancels.
//
// OnRender() each frame (kEditing):
//   - Projects control points to screen and draws coloured circles
//     (yellow = selected, white = hovered, grey = default).
//   - Draws an ImGuizmo translate gizmo on the selected point.
//   - On LMB click on a control point: selects it.
//   - On LMB click on empty terrain: inserts a new point after the current
//     selection (or appends when nothing is selected) and regenerates the mesh.
//   - Shows a small Road width slider overlay at the top-left of the viewport.
//
// OnRender() each frame (kCreating):
//   - On LMB click on terrain: creates the road on the first click, appends
//     a control point on each subsequent click, regenerates the mesh.
//   - Draws a ghost line from the last placed point to the mouse cursor.
//   - Shows a hint overlay with keyboard instructions.
//
// OnEvent() (kEditing): Delete key removes the selected control point
//   (minimum spline size of 3 points is enforced).
// OnEvent() (kCreating): Enter finalises; Escape cancels and removes the road.
//
// IsCapturingMouse() returns true when dragging or hovering a control point
// so the selection tool does not fire at the same time.
class RoadTool : public EditorToolBase {
 public:
  enum class Mode { kEditing, kCreating };

  // Enters kEditing mode; called by EditorViewport when an existing road is
  // already selected.
  void OnActivate(const EditorToolContext& ctx) override;

  // Enters kCreating mode; called by EditorWindow instead of OnActivate() when
  // the Create Road toolbar action is activated.  Must be called before
  // viewport_->SetActiveTool() so that the flag is consumed by OnActivate().
  void OnActivateCreation();

  void OnDeactivate() override;
  void OnEvent(const core::Event& event) override;
  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;
  bool IsCapturingMouse() const override;

  // Returns true while the tool is laying a new road (kCreating mode).
  // Used by EditorWindow to suppress the road auto-activation auto-logic.
  [[nodiscard]] bool IsCreating() const { return mode_ == Mode::kCreating; }

 private:
  // Projects a world-space point to screen space using the view-projection
  // matrix. Returns false when the point is behind the camera (clip.w <= 0).
  static bool ProjectPoint(const core::Vec3f& world_pos,
                           const core::Mat4f& vp,
                           ImVec2 image_pos, ImVec2 image_size,
                           ImVec2* out_screen);

  // Calls road_->RegenerateMesh() with a height function derived from
  // ctx.terrain_data (returns 0 when nullptr).
  void Regenerate(const EditorToolContext& ctx) const;

  // Renders the creation-mode overlay (hint text, ghost line, point circles).
  void RenderCreating(const EditorToolContext& ctx,
                      ImVec2 image_pos, ImVec2 image_size);

  // Renders the editing-mode overlay (width slider, point circles, gizmo).
  void RenderEditing(const EditorToolContext& ctx,
                     ImVec2 image_pos, ImVec2 image_size);

  // cppcheck-suppress unusedStructMember
  Mode mode_ = Mode::kEditing;

  // Set by OnActivateCreation(); consumed and cleared by OnActivate().
  // cppcheck-suppress unusedStructMember
  bool entering_creation_mode_ = false;

  // Non-owning pointer to the scene; set in OnActivate(), used during creation
  // to add or remove the road.
  // cppcheck-suppress unusedStructMember
  EditorScene* scene_ = nullptr;

  // Not owned; points to the road selected when OnActivate() was called, or
  // to the road created on the first click in creation mode.
  // cppcheck-suppress unusedStructMember
  game::GameRoad* road_           = nullptr;
  // Index of the selected control point (-1 = none).
  // cppcheck-suppress unusedStructMember
  int             selected_point_ = -1;
  // cppcheck-suppress unusedStructMember
  bool            dragging_       = false;
  // True when the mouse is within hover radius of any control point.
  // cppcheck-suppress unusedStructMember
  bool            hovering_       = false;
};

}  // namespace editor
