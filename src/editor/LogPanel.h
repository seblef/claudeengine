#pragma once

namespace editor {

// Bottom panel displaying recent loguru log entries.
// Full implementation in issue #178.
class LogPanel {
 public:
  LogPanel() = default;

  // Renders the log text view inside the current ImGui window.
  void Render();
};

}  // namespace editor
