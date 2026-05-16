#pragma once

#include "editor/EditorTool.h"

namespace editor {

// Horizontal toolbar with mutually exclusive tool-selector buttons.
//
// Each frame, Render() draws the transform tool buttons (with Q/W/E/R/C
// shortcuts), a visual separator, and four creation tool buttons (no shortcuts).
// EditorWindow reads GetActiveTool() each frame and forwards the result to
// EditorViewport so the gizmo and selection mode stay in sync.
class EditorToolbar {
 public:
  EditorToolbar() = default;

  // Renders the toolbar ImGui window and handles keyboard shortcuts.
  void Render();

  [[nodiscard]] EditorTool GetActiveTool() const { return active_tool_; }
  void SetActiveTool(EditorTool tool)            { active_tool_ = tool;  }

  // Convenience: true when the selection tool (no gizmo) is active.
  [[nodiscard]] bool IsSelectionToolActive() const {
    return active_tool_ == EditorTool::kSelection;
  }

 private:
  EditorTool active_tool_ = EditorTool::kSelection;
};

}  // namespace editor
