#include "editor/EditorToolbar.h"

#include <imgui.h>

namespace editor {

namespace {

struct ToolEntry { EditorTool tool; const char* label; };
constexpr ToolEntry kTools[] = {
    {EditorTool::kSelection, "Select"},
    {EditorTool::kCamera,    "Camera"},
    {EditorTool::kTranslate, "Move"},
    {EditorTool::kRotate,    "Rotate"},
    {EditorTool::kScale,     "Scale"},
};
constexpr int kToolCount = static_cast<int>(sizeof(kTools) / sizeof(kTools[0]));

}  // namespace

void EditorToolbar::Render() {
    constexpr ImGuiWindowFlags kFlags =
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (!ImGui::Begin("Toolbar", nullptr, kFlags)) {
        ImGui::End();
        return;
    }

    for (int i = 0; i < kToolCount; ++i) {
        const bool active = (active_tool_ == kTools[i].tool);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(kTools[i].label))
            active_tool_ = kTools[i].tool;
        if (active)
            ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    ImGui::End();
}

}  // namespace editor
