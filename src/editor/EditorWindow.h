#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "abstract/VideoDevice.h"
#include "audio/ResourceManager.h"
#include "audio/SoundManager.h"
#include "core/Event.h"
#include "editor/EditorCommand.h"
#include "editor/EditorCommandHistory.h"
#include "editor/ResourcePanelRegistry.h"
#include "editor/EditorToolbar.h"
#include "editor/EnvironmentEditorPanel.h"
#include "editor/PlayModeManager.h"
#include "editor/RenderingSettingsPanel.h"
#include "editor/TerrainCreationDialog.h"
#include "editor/TerrainEditorPanel.h"
#include "editor/SoundPreviewPlayer.h"
#include "editor/tools/PlacementTool.h"
#include "editor/tools/RoadTool.h"
#include "editor/tools/SelectionTool.h"
#include "editor/tools/TerrainSculptTool.h"
#include "editor/tools/TransformTool.h"
#include "game/GameObject.h"

namespace editor {

class EditorScene;
class EditorViewport;
class MapPropertiesWindow;
class MaterialEditorWindow;
class MeshEditorWindow;
class ParticleEditorWindow;
class SoundEditorWindow;
class VehicleEditorWindow;
class MeshSelectionModal;
class ParticleSystemSelectionModal;
class SoundEmitterSelectionModal;
class VehicleSelectionModal;
class PropertiesPanel;
class ProfilerPanel;
class ResourceBrowser;
class ResourcesPanel;
class ObjectsPanel;
class OutlinerPanel;
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

  // Aligns the selected object to the terrain below it: places its bbox bottom
  // on the terrain surface and tilts its orientation to match the slope.
  // No-op when no terrain is present or nothing is selected.
  void FallToTerrain();

  // Groups the current multi-object selection under a new GamePivot.
  // Pushes a GroupUnderPivotCommand so the operation is undoable.
  // No-op if fewer than two objects are selected.
  void GroupUnderPivot();

  // Ungroups the selected GamePivot: reparents its children to its parent and
  // removes the pivot. Pushes an UngroupPivotCommand so it is undoable.
  // No-op if the selected object is not a GamePivot.
  void UngroupSelectedPivot();

  // Recursively copies obj and all its descendants into clipboard_/
  // clipboard_parent_names_. Non-copyable children are skipped with a warning.
  void CopySubtree(const game::GameObject* obj, const std::string& parent_name);

  // Moves the camera so the selected object's world bounding box fills the view.
  // No-op when nothing is selected or the selection is a terrain.
  void CenterCameraOnObject();

  // Pastes the clipboard object into the scene with a small position offset.
  // Pushes a PlaceObjectCommand so the paste is undoable.
  void PasteObject();

  // Activates 3D spatial audio for all GameSoundEmitter objects in the scene.
  // Wires each emitter to the editor sound manager and starts looping playback.
  void EnableSceneSound();

  // Deactivates 3D spatial audio: stops all running sound instances and
  // removes the managers from every GameSoundEmitter in the scene.
  void DisableSceneSound();

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

  // Persists physics debug draw settings to the editor section of config.yaml.
  void SavePhysicsDebugSettings();

  // Persists overlay gizmo visibility settings to the editor section of config.yaml.
  void SaveOverlaySettings();

  // Shows the "Unsaved Changes" modal if dirty, then fires on_proceed.
  // Stores on_proceed in pending_after_save_ and sets the open flag.
  void CheckDirtyThenRun(std::function<void()> on_proceed);

  // Renders the "Unsaved Changes##modal" popup every frame.
  // Fires pending_after_save_ on Save/Discard, clears it on Cancel.
  void RenderUnsavedChangesModal();

  // Builds TerrainData/TerrainMaterial from dialog params
  // and adds a GameTerrain to the scene.
  void CreateTerrain();

  // Creates a GameRoad with four default control points, adds it to the scene,
  // selects it (which auto-activates the RoadTool), and returns to kSelection.
  void CreateRoad();

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
  std::unique_ptr<ParticleEditorWindow>  particle_editor_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<SoundEditorWindow>     sound_editor_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<VehicleEditorWindow>   vehicle_editor_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<MeshSelectionModal>              mesh_modal_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ParticleSystemSelectionModal>    particle_modal_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<SoundEmitterSelectionModal>      sound_modal_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<VehicleSelectionModal>           vehicle_modal_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<PropertiesPanel>       properties_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ResourcesPanel>        resources_panel_;
  // Registry must outlive resource_browser_ (browser holds a raw pointer).
  // cppcheck-suppress unusedStructMember
  ResourcePanelRegistry                  resource_panel_registry_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ResourceBrowser>       resource_browser_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ObjectsPanel>          objects_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<OutlinerPanel>         outliner_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<LogPanel>              log_panel_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<ProfilerPanel>         profiler_panel_;
  // cppcheck-suppress unusedStructMember
  bool                                   show_profiler_panel_ = false;

  // Play-in-Editor manager — owns the FPS camera and physics lifecycle.
  // Constructed after all other members; must be destroyed before the scene.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<PlayModeManager>       play_mode_;

  // Command history for undo/redo. Panels receive a raw pointer to this.
  // cppcheck-suppress unusedStructMember
  EditorCommandHistory                   history_;

  // Persistent tools: created once in the constructor, live for the window lifetime.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<SelectionTool>         selection_tool_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<TransformTool>         translate_tool_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<TransformTool>         rotate_tool_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<TransformTool>         scale_tool_;
  // Ephemeral tools: constructed on demand, nullptr when inactive.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<PlacementTool>         placement_tool_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<TerrainSculptTool>     sculpt_tool_;
  // Road spline-editing tool: created once, activated on road selection.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<RoadTool>              road_tool_;

  // Tracks the previous frame's active tool to detect transitions.
  // cppcheck-suppress unusedStructMember
  EditorTool                             prev_tool_        = EditorTool::kSelection;
  // True on the frame File > New is clicked; triggers OpenPopup that frame.
  // cppcheck-suppress unusedStructMember
  bool                                   new_map_pending_  = false;
  // True when the Particle Editor window should be shown.
  // cppcheck-suppress unusedStructMember
  bool                                   show_particle_editor_ = false;

  // True when the Sound Editor window should be shown.
  // cppcheck-suppress unusedStructMember
  bool                                   show_sound_editor_ = false;

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

  // Audio preview player — used by the Sound Emitter properties "Play Once" button.
  // cppcheck-suppress unusedStructMember
  SoundPreviewPlayer                     sound_preview_;

  // Editor-level 3D spatial audio — active when the sound toggle is on.
  // Shares the ISoundSystem owned by sound_preview_ to avoid multiple OpenAL
  // contexts (alcMakeContextCurrent is global; two contexts cannot coexist).
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<audio::SoundManager>     editor_sound_manager_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<audio::ResourceManager>  editor_sound_resources_;

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
  bool               show_terrain_panel_  = false;
  // True while sculpt_tool_ is the viewport's active base tool.
  // cppcheck-suppress unusedStructMember
  bool               sculpt_tool_active_  = false;
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

  // Rendering settings panel — shown via the View menu.
  // cppcheck-suppress unusedStructMember
  RenderingSettingsPanel rendering_settings_panel_;
  // cppcheck-suppress unusedStructMember
  bool show_rendering_settings_panel_ = false;

  // Maximum number of entries kept in the recent maps list.
  // cppcheck-suppress unusedStructMember
  static constexpr int kMaxRecentMaps = 5;

  // Ordered list of recently opened map paths (most-recent first).
  // cppcheck-suppress unusedStructMember
  std::vector<std::string> recent_maps_;

  // Clipboard — holds master clones used to produce each paste.
  // Never added to the scene; empty when nothing has been copied.
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<game::GameObject>> clipboard_;
  // Parallel to clipboard_: the name of each entry's parent ("" = root).
  // Used by PasteObject() to recreate the hierarchy after pasting a subtree.
  // cppcheck-suppress unusedStructMember
  std::vector<std::string>                       clipboard_parent_names_;

  // Transient status bar message from PlayModeManager (warnings, errors).
  // cppcheck-suppress unusedStructMember
  std::string play_status_msg_;
  // cppcheck-suppress unusedStructMember
  float       play_status_timer_ = 0.f;

  // Debug state.
  // cppcheck-suppress unusedStructMember
  bool terrain_wireframe_debug_ = false;
};

}  // namespace editor
