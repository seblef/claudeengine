#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "abstract/VideoDevice.h"
#include "core/Event.h"
#include "editor/EditorCommand.h"
#include "editor/EditorCommandHistory.h"
#include "editor/EditorTool.h"
#include "editor/EnvironmentEditorPanel.h"
#include "editor/TerrainCreationDialog.h"
#include "editor/TerrainEditorPanel.h"
#include "game/GameObject.h"

namespace editor {

class EditorScene;
class EditorToolbar;
class EditorViewport;
class MapPropertiesWindow;
class MaterialEditorWindow;
class MeshEditorWindow;
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

  // Copies the currently selected object to the internal clipboard.
  // No-op if nothing is selected or the object type is not copyable.
  void CopySelectedObject();

  // Pastes the clipboard object into the scene with a small position offset.
  // Pushes a PlaceObjectCommand so the paste is undoable.
  void PasteObject();

  // Saves to scene_->GetFilePath(); falls through to SaveAs() when empty.
  void SaveCurrent();

  // Opens a native save dialog and writes to the chosen path.
  void SaveAs();

  // Opens a native open dialog and loads a map file.
  void LoadFromFile();

  // Loads a map from path; updates recent maps list on success.
  void LoadMap(const std::filesystem::path& path);

  // Prepends path to recent_maps_, deduplicates, trims to kMaxRecentMaps, and
  // writes the list back to config.yaml.
  void AddToRecentMaps(const std::filesystem::path& path);

  // Persists recent_maps_ to the editor section of config.yaml.
  void SaveRecentMaps();

  // Shows the "Unsaved Changes" modal if dirty, then fires on_proceed.
  // Stores on_proceed in pending_after_save_ and sets the open flag.
  void CheckDirtyThenRun(std::function<void()> on_proceed);

  // Renders the "Unsaved Changes##modal" popup every frame.
  // Fires pending_after_save_ on Save/Discard, clears it on Cancel.
  void RenderUnsavedChangesModal();

  // Builds TerrainData/TerrainMaterial from dialog params
  // and adds a GameTerrain to the scene.
  void CreateTerrain();

  // Removes the terrain from the scene and resets all terrain-related state.
  void RemoveTerrain();

  // Creates a new terrain from an imported heightmap and adds it to the scene.
  // Called by the TerrainEditorPanel callback when no terrain exists yet.
  void CreateTerrainFromImport(std::vector<uint16_t> samples,
                               int w, int h, float min_h, float max_h);

  // Connects TerrainEditorPanel and viewport sculpt callbacks to the terrain
  // object currently in scene_, if any. Resets context when no terrain exists.
  void WireTerrainPanel();

  // Advances the autosave timer; triggers a recovery save when ready.
  // Called at the top of every Render() frame.
  void TickAutosave();

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
  std::unique_ptr<MeshEditorWindow>      mesh_editor_;
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

  // Autosave state — see TickAutosave().
  // cppcheck-suppress unusedStructMember
  float                                  autosave_timer_    = 0.f;
  // cppcheck-suppress unusedStructMember
  float                                  autosave_interval_ = 120.f;
  // cppcheck-suppress unusedStructMember
  std::string                            last_autosave_msg_;
  // cppcheck-suppress unusedStructMember
  float                                  autosave_msg_timer_ = 0.f;

  // Terrain creation dialog — opened from the Add menu.
  // cppcheck-suppress unusedStructMember
  TerrainCreationDialog                  terrain_dialog_;

  // Sculpt panel — shown via the Terrain menu when a terrain is in the scene.
  // cppcheck-suppress unusedStructMember
  TerrainEditorPanel terrain_panel_;
  // cppcheck-suppress unusedStructMember
  bool               show_terrain_panel_         = false;
  // True on the frame "Remove" is clicked; triggers the confirm modal.
  // cppcheck-suppress unusedStructMember
  bool               confirm_remove_terrain_      = false;

  // Environment editor panel — shown via the Map menu.
  // cppcheck-suppress unusedStructMember
  EnvironmentEditorPanel environment_panel_;
  // cppcheck-suppress unusedStructMember
  bool                   show_environment_panel_ = false;

  // Post-process panel — shown via the View menu.
  // cppcheck-suppress unusedStructMember
  bool show_post_process_panel_ = false;

  // Maximum number of entries kept in the recent maps list.
  // cppcheck-suppress unusedStructMember
  static constexpr int kMaxRecentMaps = 5;

  // Ordered list of recently opened map paths (most-recent first).
  // cppcheck-suppress unusedStructMember
  std::vector<std::string> recent_maps_;

  // Clipboard — holds a master clone used to produce each paste.
  // Never added to the scene; nullptr when nothing has been copied.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<game::GameObject> clipboard_;

  // Debug state.
  // cppcheck-suppress unusedStructMember
  bool terrain_wireframe_debug_ = false;
};

}  // namespace editor
