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
  //   --data-path <path>  Root directory for engine data files.
  //                       Defaults to the current working directory if omitted.
  static void Init(int argc, char* argv[]);

  // Returns the root data directory set via --data-path, or "." if not provided.
  [[nodiscard]] static const std::filesystem::path& GetDataFolder();

 private:
  Config() = delete;

  static std::filesystem::path data_folder_;
};

}  // namespace core
