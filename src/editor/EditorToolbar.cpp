#include "editor/EditorToolbar.h"

#include <IconsFontAwesome6.h>
#include <imgui.h>

namespace editor {

namespace {

struct ToolEntry {
  EditorTool  tool;
  const char* label;
  const char* tooltip;
  ImGuiKey    shortcut;
};

constexpr ToolEntry kTools[] = {
  {EditorTool::kSelection, ICON_FA_ARROW_POINTER,             "Select (Q)",    ImGuiKey_Q},
  {EditorTool::kCamera,    ICON_FA_VIDEO,                     "Camera (C)",    ImGuiKey_C},
  {EditorTool::kTranslate, ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Translate (W)", ImGuiKey_W},
  {EditorTool::kRotate,    ICON_FA_ROTATE,                    "Rotate (E)",    ImGuiKey_E},
  {EditorTool::kScale,     ICON_FA_EXPAND,                    "Scale (R)",     ImGuiKey_R},
};
constexpr int kToolCount = static_cast<int>(sizeof(kTools) / sizeof(kTools[0]));

const ImVec4 kActiveColour = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

}  // namespace

void EditorToolbar::Render() {
  // Keyboard shortcuts (skip when a text input is focused).
  if (!ImGui::GetIO().WantTextInput) {
    for (int i = 0; i < kToolCount; ++i) {
      if (ImGui::IsKeyPressed(kTools[i].shortcut, /*repeat=*/false))
        active_tool_ = kTools[i].tool;
    }
  }

  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  if (!ImGui::Begin("Toolbar", nullptr, kFlags)) {
    ImGui::End();
    return;
  }

  for (int i = 0; i < kToolCount; ++i) {
    const bool active = (active_tool_ == kTools[i].tool);
    ImGui::PushID(i);
    if (active)
      ImGui::PushStyleColor(ImGuiCol_Button, kActiveColour);
    if (ImGui::Button(kTools[i].label))
      active_tool_ = kTools[i].tool;
    ImGui::SetItemTooltip("%s", kTools[i].tooltip);
    if (active)
      ImGui::PopStyleColor();
    ImGui::PopID();
    ImGui::SameLine();
  }

  ImGui::End();
}

}  // namespace editor
