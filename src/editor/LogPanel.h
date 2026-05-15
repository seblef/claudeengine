#pragma once

#include <deque>
#include <mutex>
#include <string>

#include <loguru.hpp>

namespace editor {

// Bottom panel displaying recent loguru log entries in a scrolling view.
//
// Registers as a loguru callback sink (verbosity >= INFO). Entries are
// capped at 500 lines; new entries scroll the view to the bottom.
// Thread-safe: loguru may invoke LogCallback from any thread.
class LogPanel {
 public:
  LogPanel() = default;

  // Renders the scrolling log view inside the current ImGui window.
  void Render();

  // Loguru callback — called on the logging thread; must be thread-safe.
  static void LogCallback(void* user_data, const loguru::Message& msg);

 private:
  struct LogEntry {
    // cppcheck-suppress unusedStructMember
    loguru::Verbosity verbosity;
    // cppcheck-suppress unusedStructMember
    std::string       text;
  };

  // cppcheck-suppress unusedStructMember
  std::deque<LogEntry> entries_;
  std::mutex           mutex_;
  bool                 scroll_to_bottom_ = false;
};

}  // namespace editor
