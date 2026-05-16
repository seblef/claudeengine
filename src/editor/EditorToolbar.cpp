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
  // Keyboard shortcuts for transform tools (skip when a text input is focused).
  if (!ImGui::GetIO().WantTextInput) {
    for (int i = 0; i < kTransformToolCount; ++i) {
      if (kTransformTools[i].shortcut != ImGuiKey_None &&
          ImGui::IsKeyPressed(kTransformTools[i].shortcut, /*repeat=*/false)) {
        active_tool_ = kTransformTools[i].tool;
      }
    }
  }

  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  if (!ImGui::Begin("Toolbar", nullptr, kFlags)) {
    ImGui::End();
    return;
  }

  for (int i = 0; i < kTransformToolCount; ++i)
    RenderToolButton(kTransformTools[i], active_tool_, i);

  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  for (int i = 0; i < kCreationToolCount; ++i)
    RenderToolButton(kCreationTools[i], active_tool_, kTransformToolCount + i);

  ImGui::End();
}

}  // namespace editor
