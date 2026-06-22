#include "editor/ProfilerPanel.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "core/GpuProfiler.h"
#include "core/Profiler.h"

namespace editor {

namespace {

ImVec4 ColourForAvgMs(double avg_ms) {
  if (avg_ms < 2.0)  return {0.3f, 1.0f, 0.3f, 1.0f};
  if (avg_ms < 8.0)  return {1.0f, 1.0f, 0.3f, 1.0f};
  return {1.0f, 0.3f, 0.3f, 1.0f};
}

constexpr ImVec4 kDimWhite  = {0.6f, 0.6f, 0.6f, 1.0f};
constexpr ImVec4 kDimYellow = {0.6f, 0.6f, 0.3f, 1.0f};

// One display row merging a CPU and/or GPU sample for the same scope name.
struct MergedRow {
  std::string                name;
  const core::ProfileSample* cpu       = nullptr;
  const core::ProfileSample* gpu       = nullptr;
  double                     sort_key  = 0.0;
};

}  // namespace

// cppcheck-suppress functionStatic
void ProfilerPanel::Render() {
  const bool cpu_active = core::Profiler::IsInstanced();
  const bool gpu_active = core::GpuProfiler::IsInstanced();

  if (!cpu_active && !gpu_active) {
    ImGui::TextDisabled("Profiler not active (play mode only)");
    return;
  }

  std::vector<core::ProfileSample> cpu_report;
  std::vector<core::ProfileSample> gpu_report;
  double fps             = 0.0;
  double report_interval = 0.0;

  if (cpu_active) {
    const auto& prof = core::Profiler::Instance();
    cpu_report       = prof.GetLastReport();
    fps              = prof.GetLastFps();
    report_interval  = prof.GetReportInterval();
  }
  if (gpu_active) {
    gpu_report = core::GpuProfiler::Instance().GetLastReport();
  }

  const bool cpu_ready = !cpu_report.empty();
  const bool gpu_ready = !gpu_report.empty();

  if (!cpu_ready && !gpu_ready) {
    ImGui::TextUnformatted("Waiting for first report...");
    return;
  }

  ImGui::Text("FPS: %.1f  |  Report interval: %.1f s", fps, report_interval);
  ImGui::Spacing();

  // Build merged rows: union of CPU and GPU scope names.
  std::unordered_map<std::string, MergedRow> merged_map;
  for (const auto& s : cpu_report) {
    auto& row     = merged_map[s.name];
    row.name      = s.name;
    row.cpu       = &s;
    row.sort_key  = s.total_ms;
  }
  for (const auto& s : gpu_report) {
    auto& row = merged_map[s.name];
    row.name  = s.name;
    row.gpu   = &s;
    if (!row.cpu) row.sort_key = s.total_ms;
  }

  std::vector<const MergedRow*> rows;
  rows.reserve(merged_map.size());
  for (const auto& [_, row] : merged_map) rows.push_back(&row);
  std::sort(rows.begin(), rows.end(),
            [](const MergedRow* a, const MergedRow* b) {
              return a->sort_key > b->sort_key;
            });

  constexpr ImGuiTableFlags kTableFlags =
      ImGuiTableFlags_Borders        |
      ImGuiTableFlags_RowBg          |
      ImGuiTableFlags_SizingStretchProp;

  if (!ImGui::BeginTable("##profiler_table", 7, kTableFlags)) return;

  ImGui::TableSetupColumn("Scope",       ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("CPU avg ms",  ImGuiTableColumnFlags_WidthFixed, 78.f);
  ImGui::TableSetupColumn("CPU min ms",  ImGuiTableColumnFlags_WidthFixed, 78.f);
  ImGui::TableSetupColumn("CPU max ms",  ImGuiTableColumnFlags_WidthFixed, 78.f);
  ImGui::TableSetupColumn("GPU avg ms",  ImGuiTableColumnFlags_WidthFixed, 78.f);
  ImGui::TableSetupColumn("GPU min ms",  ImGuiTableColumnFlags_WidthFixed, 78.f);
  ImGui::TableSetupColumn("GPU max ms",  ImGuiTableColumnFlags_WidthFixed, 78.f);
  ImGui::TableHeadersRow();

  for (const MergedRow* row : rows) {
    ImGui::TableNextRow();

    // Scope name: colour by the dominant (highest avg) side.
    const double dominant_avg = row->cpu  ? row->cpu->avg_ms
                              : row->gpu  ? row->gpu->avg_ms
                              : 0.0;
    const ImVec4 name_col = ColourForAvgMs(dominant_avg);

    ImGui::TableSetColumnIndex(0);
    ImGui::TextColored(name_col, "%s", row->name.c_str());

    // CPU columns.
    if (row->cpu) {
      const ImVec4 col = ColourForAvgMs(row->cpu->avg_ms);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextColored(col, "%.3f", row->cpu->avg_ms);
      ImGui::TableSetColumnIndex(2);
      ImGui::TextColored(col, "%.3f", row->cpu->min_ms);
      ImGui::TableSetColumnIndex(3);
      ImGui::TextColored(col, "%.3f", row->cpu->max_ms);
    } else {
      ImGui::TableSetColumnIndex(1); ImGui::TextColored(kDimWhite, "—");
      ImGui::TableSetColumnIndex(2); ImGui::TextColored(kDimWhite, "—");
      ImGui::TableSetColumnIndex(3); ImGui::TextColored(kDimWhite, "—");
    }

    // GPU columns.
    if (row->gpu) {
      const ImVec4 col = ColourForAvgMs(row->gpu->avg_ms);
      ImGui::TableSetColumnIndex(4);
      ImGui::TextColored(col, "%.3f", row->gpu->avg_ms);
      ImGui::TableSetColumnIndex(5);
      ImGui::TextColored(col, "%.3f", row->gpu->min_ms);
      ImGui::TableSetColumnIndex(6);
      ImGui::TextColored(col, "%.3f", row->gpu->max_ms);
    } else {
      ImGui::TableSetColumnIndex(4); ImGui::TextColored(kDimYellow, "—");
      ImGui::TableSetColumnIndex(5); ImGui::TextColored(kDimYellow, "—");
      ImGui::TableSetColumnIndex(6); ImGui::TextColored(kDimYellow, "—");
    }
  }

  ImGui::EndTable();
}

}  // namespace editor
