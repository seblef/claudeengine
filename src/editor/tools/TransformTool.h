#pragma once

#include <vector>

#include <imgui.h>
#include <ImGuizmo.h>

#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "editor/tools/EditorToolBase.h"

namespace game { class GameObject; }

namespace editor {

// Editor tool that wraps a single ImGuizmo transform operation (translate,
// rotate, or scale). Supports both single-object and multi-selection.
//
// For a single selected object, the gizmo is placed at the object's own
// transform and the object is modified in-place (existing behaviour).
//
// For multiple selected objects, the gizmo is placed at the centre of their
// combined world bounding-box. Each frame during a drag every object is
// updated by composing the pivot delta with its stored drag-start transform:
//   new_T[i] = pivot_after * T(-centre) * T_before[i]
// On drag-end a MultiTransformCommand is pushed for atomic undo/redo.
//
// Translation applies the same vector to all objects.
// Rotation and scale apply around the group centre (pivot point).
class TransformTool : public EditorToolBase {
 public:
  explicit TransformTool(ImGuizmo::OPERATION op);

  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;

  // Returns true while the ImGuizmo gizmo is being dragged so that callers
  // can suppress other mouse interactions on the same frame.
  bool IsCapturingMouse() const override;

 private:
  ImGuizmo::OPERATION      op_;
  bool                     gizmo_was_using_  = false;
  // Per-drag snapshots — populated when the drag starts, cleared after.
  // cppcheck-suppress unusedStructMember
  core::Vec3f              pivot_center_;
  // cppcheck-suppress unusedStructMember
  std::vector<game::GameObject*>  drag_objects_;
  // cppcheck-suppress unusedStructMember
  std::vector<core::Mat4f>        drag_before_;
};

}  // namespace editor
