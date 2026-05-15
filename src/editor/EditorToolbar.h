#pragma once

#include "editor/EditorTool.h"

namespace editor {

// Toolbar providing tool-selection buttons: selection, camera, translate,
// rotate, and scale. Full implementation in issue #174.
class EditorToolbar {
 public:
  EditorToolbar() = default;

  // Renders the toolbar ImGui window.
  void Render();

  // Returns the currently active tool (stub: always kSelection until #174).
  [[nodiscard]] EditorTool GetActiveTool() const { return active_tool_; }

  // Returns true when the Selection tool is active.
  [[nodiscard]] bool IsSelectionToolActive() const {
    return active_tool_ == EditorTool::kSelection;
  }

 private:
  EditorTool active_tool_ = EditorTool::kSelection;
};

}  // namespace editor
