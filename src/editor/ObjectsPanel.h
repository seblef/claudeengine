#pragma once

namespace editor {

class EditorScene;

// Left panel "Objects" tab: tree view of scene objects grouped by type.
//
// Meshes, lights, and cameras are shown in separate collapsible groups with
// coloured inline icons. Clicking a leaf item selects the object via
// EditorScene::SetSelectedObject(); the selected object is highlighted.
class ObjectsPanel {
 public:
  ObjectsPanel() = default;

  // Renders the objects tree inside the current ImGui tab item.
  void Render(EditorScene& scene);
};

}  // namespace editor
