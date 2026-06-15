#pragma once

#include <imgui.h>
#include <ImGuizmo.h>

#include "core/Mat4f.h"
#include "editor/tools/EditorToolBase.h"

namespace editor {

// Editor tool that wraps a single ImGuizmo transform operation (translate,
// rotate, or scale). Handles gizmo rendering, drag-start / drag-end
// bookkeeping, TransformCommand recording for undo/redo, and allows
// selecting a different object by clicking outside the gizmo.
class TransformTool : public EditorToolBase {
 public:
  explicit TransformTool(ImGuizmo::OPERATION op);

  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;

  // Returns true while the ImGuizmo gizmo is being dragged so that callers
  // can suppress other mouse interactions on the same frame.
  bool IsCapturingMouse() const override;

 private:
  ImGuizmo::OPERATION op_;
  bool                gizmo_was_using_       = false;
  core::Mat4f         gizmo_before_transform_;
};

}  // namespace editor
