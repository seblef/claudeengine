#pragma once

#include <memory>

namespace editor {

class EditorToolbar;
class EditorViewport;
class ResourcesPanel;
class ObjectsPanel;
class LogPanel;

// Main editor window.
//
// Each frame, Render() builds the full ImGui docking layout:
//   - full-screen DockSpace as the root
//   - main menu bar
//   - toolbar
//   - Viewport, Scene (Resources/Objects tabs), Properties, and Logs dockable panels
//   - fixed status bar pinned to the bottom of the screen
//
// Lifecycle: owned by EditorSystem; Render() is called between
// ImGui::NewFrame() and ImGui::Render().
class EditorWindow {
 public:
  EditorWindow();
  ~EditorWindow();

  // Renders the full docking layout and all child panels.
  void Render();

 private:
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorToolbar>  toolbar_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorViewport> viewport_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ResourcesPanel> resources_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ObjectsPanel>   objects_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<LogPanel>       log_panel_;
};

}  // namespace editor
