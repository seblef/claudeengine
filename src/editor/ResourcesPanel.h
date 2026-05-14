#pragma once

namespace editor {

// Left panel "Resources" tab: tree view of loaded material and mesh templates.
// Full implementation in issue #175.
class ResourcesPanel {
 public:
  ResourcesPanel() = default;

  // Renders the resources tree inside the current ImGui tab.
  void Render();
};

}  // namespace editor
