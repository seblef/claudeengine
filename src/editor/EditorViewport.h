#pragma once

namespace editor {

// Viewport panel: renders the scene to a texture and displays it inside an
// ImGui window. Full implementation in issue #169.
class EditorViewport {
 public:
  EditorViewport() = default;

  // Renders the viewport content inside the current ImGui window.
  void Render();
};

}  // namespace editor
