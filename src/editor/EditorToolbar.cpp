#include "editor/EditorToolbar.h"

#include <IconsFontAwesome6.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace editor {

namespace {

struct ToolEntry {
  EditorTool  tool;
  const char* label;
  const char* tooltip;
  ImGuiKey    shortcut;   // ImGuiKey_None = no shortcut
};

// Transform tools — have keyboard shortcuts.
constexpr ToolEntry kTransformTools[] = {
  {EditorTool::kSelection, ICON_FA_ARROW_POINTER,             "Select (Q)",    ImGuiKey_Q},
  {EditorTool::kCamera,    ICON_FA_VIDEO,                     "Camera (C)",    ImGuiKey_C},
  {EditorTool::kTranslate, ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Translate (W)", ImGuiKey_W},
  {EditorTool::kRotate,    ICON_FA_ROTATE,                    "Rotate (E)",    ImGuiKey_E},
  {EditorTool::kScale,     ICON_FA_EXPAND,                    "Scale (R)",     ImGuiKey_R},
};
constexpr int kTransformToolCount =
    static_cast<int>(sizeof(kTransformTools) / sizeof(kTransformTools[0]));

// Creation tools — toolbar-only, no keyboard shortcuts.
constexpr ToolEntry kCreationTools[] = {
  {EditorTool::kCreateMesh,           ICON_FA_CUBE,           "Create Mesh",               ImGuiKey_None},
  {EditorTool::kCreateOmniLight,      ICON_FA_LIGHTBULB,      "Create Omni Light",         ImGuiKey_None},
  {EditorTool::kCreateCircleSpot,     ICON_FA_CIRCLE_DOT,     "Create Circle Spot Light",  ImGuiKey_None},
  {EditorTool::kCreateRectSpot,       ICON_FA_SQUARE,         "Create Rect Spot Light",    ImGuiKey_None},
  {EditorTool::kCreatePivot,          ICON_FA_LOCATION_CROSSHAIRS, "Create Pivot",          ImGuiKey_None},
  {EditorTool::kCreatePlayerStart,    ICON_FA_FLAG,           "Create Player Start",       ImGuiKey_None},
  {EditorTool::kCreateParticleSystem, ICON_FA_FIRE,           "Create Particle System",    ImGuiKey_None},
  {EditorTool::kCreateSoundEmitter,   ICON_FA_VOLUME_HIGH,    "Create Sound Emitter",      ImGuiKey_None},
  {EditorTool::kCreateVehicle,        ICON_FA_CAR,            "Create Vehicle",            ImGuiKey_None},
};
constexpr int kCreationToolCount =
    static_cast<int>(sizeof(kCreationTools) / sizeof(kCreationTools[0]));

const ImVec4 kActiveColour = ImVec4(0.184f, 0.769f, 0.698f, 1.0f);  // teal accent #2FC4B2

void RenderToolButton(const ToolEntry& entry, EditorTool& active_tool, int id) {
  const bool active = (active_tool == entry.tool);
  ImGui::PushID(id);
  if (active)
    ImGui::PushStyleColor(ImGuiCol_Button, kActiveColour);
  if (ImGui::Button(entry.label))
    active_tool = entry.tool;
  ImGui::SetItemTooltip("%s", entry.tooltip);
  if (active)
    ImGui::PopStyleColor();
  ImGui::PopID();
  ImGui::SameLine();
}

}  // namespace

void EditorToolbar::Render() {
  const ImGuiIO& io = ImGui::GetIO();

  // Keyboard shortcuts — skip when a text input widget has keyboard focus.
  if (!io.WantCaptureKeyboard) {
    // Single-key tool shortcuts are suppressed when a modifier is held so that
    // Ctrl+C (copy) does not simultaneously activate the Camera tool.
    for (int i = 0; i < kTransformToolCount; ++i) {
      if (kTransformTools[i].shortcut != ImGuiKey_None &&
          !io.KeyCtrl && !io.KeyAlt &&
          ImGui::IsKeyPressed(kTransformTools[i].shortcut, /*repeat=*/false)) {
        active_tool_ = kTransformTools[i].tool;
      }
    }

    if (history_) {
      if (io.KeyCtrl && !io.KeyShift &&
          ImGui::IsKeyPressed(ImGuiKey_Z, /*repeat=*/false)) {
        history_->Undo();
      } else if (io.KeyCtrl && io.KeyShift &&
                 ImGui::IsKeyPressed(ImGuiKey_Z, /*repeat=*/false)) {
        history_->Redo();
      }
    }

    if (io.KeyCtrl && !io.KeyShift &&
        ImGui::IsKeyPressed(ImGuiKey_C, /*repeat=*/false)) {
      if (on_copy_) on_copy_();
    }
    if (io.KeyCtrl && !io.KeyShift &&
        ImGui::IsKeyPressed(ImGuiKey_V, /*repeat=*/false)) {
      if (on_paste_) on_paste_();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F5, /*repeat=*/false)) {
      if (in_play_mode_) {
        if (on_stop_) on_stop_();
      } else if (play_enabled_) {
        if (on_play_) on_play_();
      }
    }
    if (in_play_mode_ && ImGui::IsKeyPressed(ImGuiKey_Escape, /*repeat=*/false)) {
      if (on_stop_) on_stop_();
    }
  }

  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  if (!ImGui::Begin("Toolbar", nullptr, kFlags)) {
    ImGui::End();
    return;
  }

  // Undo / Redo buttons — disabled when the history cannot perform the action.
  if (history_) {
    const bool can_undo = history_->CanUndo();
    ImGui::BeginDisabled(!can_undo);
    if (ImGui::Button(ICON_FA_ROTATE_LEFT))
      history_->Undo();
    ImGui::SetItemTooltip("Undo (Ctrl+Z)");
    ImGui::EndDisabled();

    ImGui::SameLine();

    const bool can_redo = history_->CanRedo();
    ImGui::BeginDisabled(!can_redo);
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT))
      history_->Redo();
    ImGui::SetItemTooltip("Redo (Ctrl+Shift+Z)");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
  }

  // Save button — greyed out when the scene has no unsaved changes.
  ImGui::BeginDisabled(!dirty_);
  if (ImGui::Button(ICON_FA_FLOPPY_DISK) && on_save_)
    on_save_();
  ImGui::SetItemTooltip("Save (Ctrl+S)");
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // Play / Stop button.
  ImGui::BeginDisabled(!play_enabled_ && !in_play_mode_);
  if (in_play_mode_) {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.5f, 0.05f, 0.05f, 1.0f));
    if (ImGui::Button(ICON_FA_STOP " Stop") && on_stop_) on_stop_();
    ImGui::SetItemTooltip("Stop (F5 / Escape)");
    ImGui::PopStyleColor(3);
  } else {
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.1f, 0.55f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.75f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.05f, 0.4f, 0.05f, 1.0f));
    if (ImGui::Button(ICON_FA_PLAY " Play") && on_play_) on_play_();
    ImGui::SetItemTooltip("Play (F5)");
    ImGui::PopStyleColor(3);
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // Copy / Paste buttons.
  ImGui::BeginDisabled(!can_copy_);
  if (ImGui::Button(ICON_FA_COPY) && on_copy_) on_copy_();
  ImGui::SetItemTooltip("Copy (Ctrl+C)");
  ImGui::EndDisabled();

  ImGui::SameLine();

  ImGui::BeginDisabled(!can_paste_);
  if (ImGui::Button(ICON_FA_PASTE) && on_paste_) on_paste_();
  ImGui::SetItemTooltip("Paste (Ctrl+V)");
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  for (int i = 0; i < kTransformToolCount; ++i)
    RenderToolButton(kTransformTools[i], active_tool_, i);

  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  for (int i = 0; i < kCreationToolCount; ++i)
    RenderToolButton(kCreationTools[i], active_tool_, kTransformToolCount + i);

  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // Action tools — one-shot buttons (not toggle modes).
  ImGui::BeginDisabled(!can_fall_to_terrain_);
  if (ImGui::Button(ICON_FA_CIRCLE_ARROW_DOWN) && on_fall_to_terrain_)
    on_fall_to_terrain_();
  ImGui::SetItemTooltip("Fall to Terrain");
  if (!can_fall_to_terrain_)
    ImGui::SetItemTooltip("Select an object and add a terrain first");
  ImGui::EndDisabled();

  ImGui::SameLine();

  ImGui::BeginDisabled(!can_center_on_object_);
  if (ImGui::Button(ICON_FA_CROSSHAIRS) && on_center_on_object_)
    on_center_on_object_();
  ImGui::SetItemTooltip("Center Camera on Object");
  if (!can_center_on_object_)
    ImGui::SetItemTooltip("Select a non-terrain object first");
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // Group action buttons.
  ImGui::BeginDisabled(!can_group_);
  if (ImGui::Button(ICON_FA_OBJECT_GROUP) && on_group_objects_)
    on_group_objects_();
  ImGui::SetItemTooltip("Group selected objects");
  if (!can_group_)
    ImGui::SetItemTooltip("Select 2 or more ungrouped objects to group");
  ImGui::EndDisabled();

  ImGui::SameLine();

  ImGui::BeginDisabled(!can_ungroup_);
  if (ImGui::Button(ICON_FA_OBJECT_UNGROUP) && on_ungroup_objects_)
    on_ungroup_objects_();
  ImGui::SetItemTooltip("Ungroup");
  if (!can_ungroup_)
    ImGui::SetItemTooltip("Select a group to ungroup");
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // Sound enable/disable toggle.
  // Capture state before the button so Push/Pop are balanced regardless of click.
  const bool sound_was_enabled = sound_enabled_;
  if (sound_was_enabled)
    ImGui::PushStyleColor(ImGuiCol_Button, kActiveColour);
  if (ImGui::Button(sound_was_enabled ? ICON_FA_VOLUME_HIGH : ICON_FA_VOLUME_XMARK)) {
    sound_enabled_ = !sound_enabled_;
    if (on_sound_toggle_) on_sound_toggle_(sound_enabled_);
  }
  ImGui::SetItemTooltip(sound_enabled_ ? "Sound enabled (click to disable)"
                                       : "Sound disabled (click to enable)");
  if (sound_was_enabled)
    ImGui::PopStyleColor();

  ImGui::End();
}

}  // namespace editor
