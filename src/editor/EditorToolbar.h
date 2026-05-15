#pragma once

namespace editor {

// Toolbar providing tool-selection buttons: selection, camera, translate,
// rotate, and scale. Full implementation in issue #174.
class EditorToolbar {
 public:
  EditorToolbar() = default;

  // Renders the toolbar ImGui window.
  void Render();

  // Returns true when the Selection tool is the active tool.
  // Stub: always true until issue #174 implements full tool selection.
  [[nodiscard]] bool IsSelectionToolActive() const { return true; }
};

}  // namespace editor
