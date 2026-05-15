#pragma once

namespace editor {

class EditorScene;

// Left panel "Resources" tab: tree view of loaded materials and mesh templates.
//
// Materials come from EditorScene::GetMaterials(); mesh templates from the
// MeshTemplate static registry via MeshTemplate::GetAll(). Clicking a leaf
// item is a no-op in this milestone.
class ResourcesPanel {
 public:
  ResourcesPanel() = default;

  // Renders the resources tree inside the current ImGui tab item.
  void Render(const EditorScene& scene);
};

}  // namespace editor
