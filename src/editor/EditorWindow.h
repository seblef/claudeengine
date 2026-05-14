#pragma once

namespace editor {

// Main editor docking window.
//
// Owns and renders all editor panels within the ImGui docking layout:
// the viewport, left resource/object panels, right properties panel,
// and the bottom log panel.
//
// Lifecycle: owned by EditorSystem; Render() is called once per frame
// between ImGui::NewFrame() and ImGui::Render().
class EditorWindow {
 public:
  EditorWindow() = default;

  // Renders the full docking layout and all child panels.
  void Render();
};

}  // namespace editor
