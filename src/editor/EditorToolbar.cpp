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
  {EditorTool::kCreateMesh,       ICON_FA_CUBE,        "Create Mesh",               ImGuiKey_None},
  {EditorTool::kCreateOmniLight,  ICON_FA_LIGHTBULB,   "Create Omni Light",         ImGuiKey_None},
  {EditorTool::kCreateCircleSpot, ICON_FA_CIRCLE_DOT,  "Create Circle Spot Light",  ImGuiKey_None},
  {EditorTool::kCreateRectSpot,   ICON_FA_SQUARE,      "Create Rect Spot Light",    ImGuiKey_None},
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
    for (int i = 0; i < kTransformToolCount; ++i) {
      if (kTransformTools[i].shortcut != ImGuiKey_None &&
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

  for (int i = 0; i < kTransformToolCount; ++i)
    RenderToolButton(kTransformTools[i], active_tool_, i);

  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  for (int i = 0; i < kCreationToolCount; ++i)
    RenderToolButton(kCreationTools[i], active_tool_, kTransformToolCount + i);

  ImGui::End();
}

}  // namespace editor
