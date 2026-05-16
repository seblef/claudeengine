#pragma once

#include "editor/EditorCommandHistory.h"
#include "editor/EditorTool.h"

namespace editor {

// Horizontal toolbar with mutually exclusive tool-selector buttons.
//
// Each frame, Render() draws undo/redo buttons, a separator, the transform tool
// buttons (with Q/W/E/R/C shortcuts), another separator, and four creation tool
// buttons (no shortcuts). EditorWindow reads GetActiveTool() each frame and
// forwards the result to EditorViewport so the gizmo and selection mode stay in
// sync.
//
// Call SetCommandHistory() once after construction to enable undo/redo buttons
// and Ctrl+Z / Ctrl+Shift+Z shortcuts.
class EditorToolbar {
 public:
  EditorToolbar() = default;

  // Renders the toolbar ImGui window and handles keyboard shortcuts.
  void Render();

  // Wires the command history for undo/redo. Must be called before Render().
  void SetCommandHistory(EditorCommandHistory* history) { history_ = history; }

  [[nodiscard]] EditorTool GetActiveTool() const { return active_tool_; }
  void SetActiveTool(EditorTool tool)            { active_tool_ = tool;  }

  // Convenience: true when the selection tool (no gizmo) is active.
  [[nodiscard]] bool IsSelectionToolActive() const {
    return active_tool_ == EditorTool::kSelection;
  }

 private:
  EditorTool            active_tool_ = EditorTool::kSelection;
  EditorCommandHistory* history_     = nullptr;
};

}  // namespace editor
