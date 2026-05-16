#pragma once

#include <memory>

#include "abstract/VideoDevice.h"
#include "core/Event.h"
#include "editor/EditorTool.h"

namespace editor {

class EditorScene;
class EditorToolbar;
class EditorViewport;
class MaterialEditorWindow;
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
  explicit EditorWindow(abstract::VideoDevice* video);
  ~EditorWindow();

  // Renders the full docking layout and all child panels.
  void Render();

  // Forwards a platform event to child panels that need input.
  void OnEvent(const core::Event& event);

 private:
  void RenderMenuBar();
  void ImportMaterial();
  void ImportMesh();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // scene_ must be declared before viewport_ so it is created first and
  // destroyed after (viewport_ holds a non-owning pointer to it).
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorScene>           scene_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorToolbar>         toolbar_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorViewport>        viewport_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<MaterialEditorWindow>  material_editor_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ResourcesPanel>        resources_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ObjectsPanel>          objects_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<LogPanel>              log_panel_;

  // Tracks the previous frame's active tool to detect transitions.
  // cppcheck-suppress unusedStructMember
  EditorTool                             prev_tool_      = EditorTool::kSelection;
};

}  // namespace editor
