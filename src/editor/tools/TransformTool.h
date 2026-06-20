#pragma once

#include <vector>

#include <imgui.h>
#include <ImGuizmo.h>

#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "editor/tools/EditorToolBase.h"
#include "editor/tools/PickingUtils.h"

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
//
// While the mouse hovers over the viewport, a white semi-transparent wireframe
// bbox is drawn around the object under the cursor (hover feedback). The ray
// cast is cached per mouse position to avoid per-frame cost.
class TransformTool : public EditorToolBase {
 public:
  explicit TransformTool(ImGuizmo::OPERATION op);

  void OnDeactivate() override;
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
  // Accumulated pivot matrix in ImGuizmo's float[16] format.
  // Persisted across drag frames so rotation and scale accumulate correctly
  // instead of being reset to identity orientation each frame.
  // cppcheck-suppress unusedStructMember
  float                    pivot_im_[16]     = {};
  // cppcheck-suppress unusedStructMember
  std::vector<game::GameObject*>  drag_objects_;
  // cppcheck-suppress unusedStructMember
  std::vector<core::Mat4f>        drag_before_;

  // Hover state: last ray-cast result and the mouse position it was cast from.
  // cppcheck-suppress unusedStructMember
  game::GameObject* hovered_object_       = nullptr;
  // cppcheck-suppress unusedStructMember
  ImVec2            last_hover_mouse_pos_ = {-1.f, -1.f};
};

}  // namespace editor
