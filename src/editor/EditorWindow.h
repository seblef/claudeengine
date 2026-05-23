#pragma once

#include <functional>
#include <memory>

#include "abstract/VideoDevice.h"
#include "core/Event.h"
#include "editor/EditorCommand.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorTool.h"

namespace editor {

class EditorScene;
class EditorToolbar;
class EditorViewport;
class MapPropertiesWindow;
class MaterialEditorWindow;
class MeshSelectionModal;
class PropertiesPanel;
class ResourcesPanel;
class ObjectsPanel;
class LogPanel;

// Main editor window.
//
// Each frame, Render() builds the full ImGui docking layout:
//   - full-screen DockSpace as the root
//   - main menu bar (File, Map, Import) with keyboard shortcuts Ctrl+S/Ctrl+N
//   - toolbar (undo/redo, save, transform and creation tools)
//   - Viewport, Scene (Resources/Objects tabs), Properties, and Logs dockable panels
//   - fixed status bar pinned to the bottom of the screen
//
// File operations (New, Load, Save, Save As) track a dirty flag so the user is
// warned about unsaved changes before destructive actions.
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

  // Saves to scene_->GetFilePath(); falls through to SaveAs() when empty.
  void SaveCurrent();

  // Opens a native save dialog and writes to the chosen path.
  void SaveAs();

  // Opens a native open dialog and loads a map file.
  void LoadFromFile();

  // Shows the "Unsaved Changes" modal if dirty, then fires on_proceed.
  // Stores on_proceed in pending_after_save_ and sets the open flag.
  void CheckDirtyThenRun(std::function<void()> on_proceed);

  // Renders the "Unsaved Changes##modal" popup every frame.
  // Fires pending_after_save_ on Save/Discard, clears it on Cancel.
  void RenderUnsavedChangesModal();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // scene_ must be declared before viewport_ so it is created first and
  // destroyed after (viewport_ holds a non-owning pointer to it).
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorScene>           scene_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<MapPropertiesWindow>   map_properties_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorToolbar>         toolbar_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<EditorViewport>        viewport_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<MaterialEditorWindow>  material_editor_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<MeshSelectionModal>    mesh_modal_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<PropertiesPanel>       properties_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ResourcesPanel>        resources_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ObjectsPanel>          objects_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<LogPanel>              log_panel_;

  // Command history for undo/redo. Panels receive a raw pointer to this.
  // cppcheck-suppress unusedStructMember
  EditorCommandHistory                   history_;

  // Tracks the previous frame's active tool to detect transitions.
  // cppcheck-suppress unusedStructMember
  EditorTool                             prev_tool_        = EditorTool::kSelection;
  // True while waiting for the user to click to place a mesh in the viewport.
  // cppcheck-suppress unusedStructMember
  bool                                   placement_active_ = false;
  // True on the frame File > New is clicked; triggers OpenPopup that frame.
  // cppcheck-suppress unusedStructMember
  bool                                   new_map_pending_  = false;

  // True when scene has unsaved changes; gates File > Save and the toolbar Save button.
  // cppcheck-suppress unusedStructMember
  bool                                   scene_dirty_      = false;
  // True when the Map Properties panel should be shown.
  // cppcheck-suppress unusedStructMember
  bool                                   show_map_props_   = false;
  // Set to true to open the "Unsaved Changes" modal on the next Render() call.
  // cppcheck-suppress unusedStructMember
  bool                                   open_unsaved_changes_modal_ = false;
  // Callback to run after the user resolves the "Unsaved Changes" modal.
  // cppcheck-suppress unusedStructMember
  std::function<void()>                  pending_after_save_;
};

}  // namespace editor
