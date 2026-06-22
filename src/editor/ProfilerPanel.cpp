#include "editor/ProfilerPanel.h"

#include <imgui.h>

#include "core/Profiler.h"

namespace editor {

namespace {

ImVec4 ColourForAvgMs(double avg_ms) {
  if (avg_ms < 2.0)  return {0.3f, 1.0f, 0.3f, 1.0f};
  if (avg_ms < 8.0)  return {1.0f, 1.0f, 0.3f, 1.0f};
  return {1.0f, 0.3f, 0.3f, 1.0f};
}

}  // namespace

// cppcheck-suppress functionStatic
void ProfilerPanel::Render() {
  if (!core::Profiler::IsInstanced()) {
    ImGui::TextDisabled("Profiler not active (play mode only)");
    return;
  }

  const auto& profiler = core::Profiler::Instance();
  const std::vector<core::ProfileSample> report = profiler.GetLastReport();
  const double fps = profiler.GetLastFps();

  if (report.empty()) {
    ImGui::TextUnformatted("Waiting for first report...");
    return;
  }

  ImGui::Text("FPS: %.1f  |  Report interval: %.1f s",
              fps, profiler.GetReportInterval());
  ImGui::Spacing();

  constexpr ImGuiTableFlags kTableFlags =
      ImGuiTableFlags_Borders        |
      ImGuiTableFlags_RowBg          |
      ImGuiTableFlags_SizingStretchProp;

  if (!ImGui::BeginTable("##profiler_table", 6, kTableFlags)) return;

  ImGui::TableSetupColumn("Scope",      ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Avg (ms)",   ImGuiTableColumnFlags_WidthFixed, 72.f);
  ImGui::TableSetupColumn("Min (ms)",   ImGuiTableColumnFlags_WidthFixed, 72.f);
  ImGui::TableSetupColumn("Max (ms)",   ImGuiTableColumnFlags_WidthFixed, 72.f);
  ImGui::TableSetupColumn("Total (ms)", ImGuiTableColumnFlags_WidthFixed, 80.f);
  ImGui::TableSetupColumn("Calls",      ImGuiTableColumnFlags_WidthFixed, 52.f);
  ImGui::TableHeadersRow();

  for (const core::ProfileSample& s : report) {
    ImGui::TableNextRow();
    const ImVec4 col = ColourForAvgMs(s.avg_ms);

    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(col, "%s", s.name.c_str());

    ImGui::TableSetColumnIndex(1);
    ImGui::TextColored(col, "%.3f", s.avg_ms);

    ImGui::TableSetColumnIndex(2);
    ImGui::TextColored(col, "%.3f", s.min_ms);

    ImGui::TableSetColumnIndex(3);
    ImGui::TextColored(col, "%.3f", s.max_ms);

    ImGui::TableSetColumnIndex(4);
    ImGui::TextColored(col, "%.3f", s.total_ms);

    ImGui::TableSetColumnIndex(5);
    ImGui::TextColored(col, "%u", s.calls);
  }

  ImGui::EndTable();
}

}  // namespace editor
