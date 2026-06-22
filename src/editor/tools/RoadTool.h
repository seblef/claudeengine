#pragma once

#include "core/Event.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "editor/tools/EditorToolBase.h"

#include <imgui.h>

namespace game { class GameRoad; }

namespace editor {

// Tool for in-scene spline editing of a GameRoad object.
//
// Auto-activated by EditorWindow when a GameRoad is selected; deactivates when
// selection changes to a non-road object. Never exposed as a toolbar button.
//
// OnRender() each frame:
//   - Projects control points to screen and draws coloured circles
//     (yellow = selected, white = hovered, grey = default).
//   - Draws an ImGuizmo translate gizmo on the selected point.
//   - On LMB click on a control point: selects it.
//   - On LMB click on empty terrain: inserts a new point after the current
//     selection (or appends when nothing is selected) and regenerates the mesh.
//   - Shows a small Road width slider overlay at the top-left of the viewport.
//
// OnEvent() handles the Delete key to remove the selected control point
// (minimum spline size of 3 points is enforced).
//
// IsCapturingMouse() returns true when dragging or hovering a control point
// so the selection tool does not fire at the same time.
class RoadTool : public EditorToolBase {
 public:
  void OnActivate(const EditorToolContext& ctx) override;
  void OnDeactivate() override;
  void OnEvent(const core::Event& event) override;
  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;
  bool IsCapturingMouse() const override;

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

  // Not owned; points to the road selected when OnActivate() was called.
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
