#pragma once

namespace editor {

// Toolbar providing tool-selection buttons: selection, camera, translate,
// rotate, and scale. Full implementation in issue #174.
class EditorToolbar {
 public:
  EditorToolbar() = default;

  // Renders the toolbar ImGui window.
  void Render();
};

}  // namespace editor
