#include "editor/LogPanel.h"

#include <imgui.h>

namespace editor {

namespace {

constexpr size_t kMaxEntries = 500;

ImVec4 ColourForVerbosity(loguru::Verbosity v) {
  if (v == loguru::Verbosity_WARNING) return {1.f, 1.f, 0.f, 1.f};
  if (v <= loguru::Verbosity_ERROR)   return {1.f, 0.3f, 0.3f, 1.f};
  return {1.f, 1.f, 1.f, 1.f};
}

}  // namespace

// static
void LogPanel::LogCallback(void* user_data, const loguru::Message& msg) {
  auto* panel = static_cast<LogPanel*>(user_data);
  std::lock_guard<std::mutex> lock(panel->mutex_);
  if (panel->entries_.size() >= kMaxEntries) panel->entries_.pop_front();
  panel->entries_.push_back({msg.verbosity, msg.message});
  panel->scroll_to_bottom_ = true;
}

void LogPanel::Render() {
  std::lock_guard<std::mutex> lock(mutex_);
  ImGui::BeginChild("##logs", ImVec2(0, 0), ImGuiChildFlags_None,
                    ImGuiWindowFlags_HorizontalScrollbar);
  for (const auto& e : entries_) {
    ImGui::TextColored(ColourForVerbosity(e.verbosity), "%s", e.text.c_str());
  }
  if (scroll_to_bottom_) {
    ImGui::SetScrollHereY(1.0f);
    scroll_to_bottom_ = false;
  }
  ImGui::EndChild();
}

}  // namespace editor
