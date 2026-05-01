#pragma once

// Logging subsystem facade for ClaudeEngine.
//
// After Logger::Init(), use LOG_F / DLOG_F / LOG_SCOPE_F (from loguru.hpp)
// directly in any translation unit — no wrapper macros are needed.

namespace core {

// Static facade around loguru. Manages lifecycle: init, file sink, shutdown.
class Logger {
 public:
  // Initialises loguru, parses -v <level> from argv, and opens
  // "claude_engine.log" (Truncate — fresh file on each launch).
  // Must be called from the main thread before any LOG_F usage.
  static void Init(int argc, char* argv[]);

  // Flushes pending log entries and removes all sinks.
  // Call once before returning from main().
  static void Shutdown();

  Logger() = delete;
};

}  // namespace core
