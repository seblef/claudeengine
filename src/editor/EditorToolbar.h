#pragma once

#include <functional>

#include "editor/EditorCommandHistory.h"

namespace editor {

// Active tool in the editor toolbar.
enum class EditorTool {
  kSelection,  // click-to-select; no transform gizmo
  kCamera,     // reserved for camera-specific interactions; no gizmo
  kTranslate,
  kRotate,
  kScale,
  // Creation tools: no gizmo; viewport placement mode is active.
  kCreateMesh,
  kCreateOmniLight,
  kCreateCircleSpot,
  kCreateRectSpot,
  kCreatePivot,
  kCreatePlayerStart,
  kCreateParticleSystem,
  kCreateSoundEmitter,
  kCreateVehicle,
  kCreateRoad,
  // Spline-editing tool for GameRoad objects. Not a toolbar button — activates
  // automatically when a GameRoad is selected.
  kRoad,
};

// Returns true for any object-creation tool.
inline bool IsCreationTool(EditorTool tool) {
  return tool == EditorTool::kCreateMesh           ||
         tool == EditorTool::kCreateOmniLight      ||
         tool == EditorTool::kCreateCircleSpot     ||
         tool == EditorTool::kCreateRectSpot       ||
         tool == EditorTool::kCreatePivot          ||
         tool == EditorTool::kCreatePlayerStart    ||
         tool == EditorTool::kCreateParticleSystem ||
         tool == EditorTool::kCreateSoundEmitter   ||
         tool == EditorTool::kCreateVehicle        ||
         tool == EditorTool::kCreateRoad;
}

// Returns true for the three transform gizmo tools.
inline bool IsTransformTool(EditorTool tool) {
  return tool == EditorTool::kTranslate ||
         tool == EditorTool::kRotate    ||
         tool == EditorTool::kScale;
}

// Horizontal toolbar with mutually exclusive tool-selector buttons.
//
// Each frame, Render() draws undo/redo buttons, a Save button, a separator,
// the transform tool buttons (with Q/W/E/R/C shortcuts), another separator,
// and four creation tool buttons (no shortcuts). EditorWindow reads
// GetActiveTool() each frame and forwards the result to EditorViewport so
// the gizmo and selection mode stay in sync.
//
// Call SetCommandHistory() once after construction to enable undo/redo buttons
// and Ctrl+Z / Ctrl+Shift+Z shortcuts. Call SetOnSave() to wire the Save
// button; call SetDirty() each frame to enable or grey out the Save button.
class EditorToolbar {
 public:
  EditorToolbar() = default;

  // Renders the toolbar ImGui window and handles keyboard shortcuts.
  void Render();

  // Wires the command history for undo/redo. Must be called before Render().
  void SetCommandHistory(EditorCommandHistory* history) { history_ = history; }

  // Registers a callback fired when the Save toolbar button is clicked.
  void SetOnSave(std::function<void()> cb) { on_save_ = std::move(cb); }

  // Registers a callback fired when the Copy toolbar button is clicked or Ctrl+C.
  void SetOnCopy(std::function<void()> cb) { on_copy_ = std::move(cb); }

  // Registers a callback fired when the Paste toolbar button is clicked or Ctrl+V.
  void SetOnPaste(std::function<void()> cb) { on_paste_ = std::move(cb); }

  // Registers a callback fired when the "Fall to Terrain" action button is clicked.
  void SetOnFallToTerrain(std::function<void()> cb) {
    on_fall_to_terrain_ = std::move(cb);
  }

  // Registers a callback fired when the "Center on Object" action button is clicked.
  void SetOnCenterOnObject(std::function<void()> cb) {
    on_center_on_object_ = std::move(cb);
  }

  // Registers a callback fired when the "Group" button is clicked.
  void SetOnGroupObjects(std::function<void()> cb) {
    on_group_objects_ = std::move(cb);
  }

  // Registers a callback fired when the "Ungroup" button is clicked.
  void SetOnUngroupObjects(std::function<void()> cb) {
    on_ungroup_objects_ = std::move(cb);
  }

  // Enables or greys out the Copy button.
  void SetCanCopy(bool b) { can_copy_ = b; }

  // Enables or greys out the Paste button.
  void SetCanPaste(bool b) { can_paste_ = b; }

  // Enables or greys out the "Fall to Terrain" button.
  // Should be false when no terrain is present or nothing is selected.
  void SetCanFallToTerrain(bool b) { can_fall_to_terrain_ = b; }

  // Enables or greys out the "Center on Object" button.
  // Should be false when nothing is selected or terrain is selected.
  void SetCanCenterOnObject(bool b) { can_center_on_object_ = b; }

  // Enables or greys out the group action buttons.
  void SetCanGroup(bool b)       { can_group_ = b; }
  void SetCanUngroup(bool b)     { can_ungroup_ = b; }

  // Registers the callback fired when Play is clicked or F5 is pressed (not in play mode).
  void SetOnPlay(std::function<void()> cb) { on_play_ = std::move(cb); }

  // Registers the callback fired when Stop is clicked, F5 or Escape is pressed (in play mode).
  void SetOnStop(std::function<void()> cb) { on_stop_ = std::move(cb); }

  // Switches button appearance: true → ■ Stop, false → ▶ Play.
  void SetInPlayMode(bool playing) { in_play_mode_ = playing; }

  // Greys out the Play button. Should be false when no map is loaded or map has no file path.
  void SetPlayEnabled(bool enabled) { play_enabled_ = enabled; }

  // Registers a callback fired when the "Place Gauge" button is clicked.
  // The callback should push a PlaceGaugeCommand onto the command history.
  void SetOnPlaceGauge(std::function<void()> cb) {
    on_place_gauge_ = std::move(cb);
  }

  // Registers a callback fired when the "Clear Gauges" button is clicked.
  void SetOnClearGauges(std::function<void()> cb) {
    on_clear_gauges_ = std::move(cb);
  }

  // Enables or greys out the "Clear Gauges" button.
  // Should be false when the scene has no gauges.
  void SetCanClearGauges(bool b) { can_clear_gauges_ = b; }

  // Registers a callback fired when the sound-enable toggle is clicked.
  // The bool argument is the new enabled state.
  void SetOnSoundToggle(std::function<void(bool)> cb) {
    on_sound_toggle_ = std::move(cb);
  }

  // Sets the current sound-enable state (reflected in the toggle button).
  void SetSoundEnabled(bool enabled) { sound_enabled_ = enabled; }

  // Returns true when the sound-enable toggle is on.
  [[nodiscard]] bool IsSoundEnabled() const { return sound_enabled_; }

  // Reflects the scene dirty state; the Save button is greyed out when false.
  void SetDirty(bool dirty) { dirty_ = dirty; }

  [[nodiscard]] EditorTool GetActiveTool() const { return active_tool_; }
  void SetActiveTool(EditorTool tool)            { active_tool_ = tool;  }

  // Convenience: true when the selection tool (no gizmo) is active.
  [[nodiscard]] bool IsSelectionToolActive() const {
    return active_tool_ == EditorTool::kSelection;
  }

 private:
  EditorTool            active_tool_ = EditorTool::kSelection;
  EditorCommandHistory* history_     = nullptr;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_save_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_copy_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_paste_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_fall_to_terrain_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_center_on_object_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_group_objects_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_ungroup_objects_;
  // cppcheck-suppress unusedStructMember
  bool                  dirty_              = false;
  // cppcheck-suppress unusedStructMember
  bool                  can_copy_           = false;
  // cppcheck-suppress unusedStructMember
  bool                  can_paste_          = false;
  // cppcheck-suppress unusedStructMember
  bool                  can_fall_to_terrain_ = false;
  // cppcheck-suppress unusedStructMember
  bool                  can_center_on_object_ = false;
  // cppcheck-suppress unusedStructMember
  bool                  can_group_          = false;
  // cppcheck-suppress unusedStructMember
  bool                  can_ungroup_        = false;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_play_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_stop_;
  // cppcheck-suppress unusedStructMember
  bool                  in_play_mode_       = false;
  // cppcheck-suppress unusedStructMember
  bool                  play_enabled_       = false;
  // cppcheck-suppress unusedStructMember
  std::function<void(bool)> on_sound_toggle_;
  // cppcheck-suppress unusedStructMember
  bool                  sound_enabled_      = false;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_place_gauge_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_clear_gauges_;
  // cppcheck-suppress unusedStructMember
  bool                  can_clear_gauges_   = false;
};

}  // namespace editor
