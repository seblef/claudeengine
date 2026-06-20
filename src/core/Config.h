#pragma once

#include <filesystem>

namespace core {

// Engine-wide configuration parsed from command-line arguments at startup.
//
// Call Init() once from main() before any other module accesses configuration.
// All methods are safe to call from any thread after Init() returns.
class Config {
 public:
  // Parses command-line arguments and stores engine configuration.
  // Recognised arguments:
  //   --data-path <path>      Root directory for engine data files.
  //                           Defaults to the current working directory.
  //   --enable-profiling      Enable the CPU profiler (presence is the signal).
  //   --profile-interval <s>  Profiler report interval in seconds (default 5.0).
  //                           Ignored if profiling is disabled.
  static void Init(int argc, char* argv[]);

  // Returns the root data directory set via --data-path, or "." if not provided.
  [[nodiscard]] static const std::filesystem::path& GetDataFolder();

  // Returns true if --enable-profiling was passed on the command line.
  [[nodiscard]] static bool IsProfilingEnabled();

  // Returns the profiler report interval in seconds (default 5.0).
  [[nodiscard]] static double GetProfileInterval();

 private:
  Config() = delete;

  static std::filesystem::path data_folder_;
  // cppcheck-suppress unusedStructMember
  static bool   profiling_enabled_;
  // cppcheck-suppress unusedStructMember
  static double profile_interval_s_;
};

}  // namespace core
