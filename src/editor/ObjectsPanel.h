#pragma once

namespace editor {

// Left panel "Objects" tab: tree view of game objects in the scene.
// Full implementation in issue #176.
class ObjectsPanel {
 public:
  ObjectsPanel() = default;

  // Renders the objects tree inside the current ImGui tab.
  void Render();
};

}  // namespace editor
