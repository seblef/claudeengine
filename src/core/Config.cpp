#include "core/Config.h"

#include <string>
#include <string_view>

namespace core {

std::filesystem::path Config::data_folder_      = ".";
bool                  Config::profiling_enabled_ = false;
double                Config::profile_interval_s_ = 5.0;

// cppcheck-suppress constParameter
void Config::Init(int argc, char* argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--data-path" && i + 1 < argc) {
      data_folder_ = argv[++i];
    } else if (arg == "--enable-profiling") {
      profiling_enabled_ = true;
    } else if (arg == "--profile-interval" && i + 1 < argc) {
      profile_interval_s_ = std::stod(argv[++i]);
    }
  }
}

const std::filesystem::path& Config::GetDataFolder() {
  return data_folder_;
}

bool Config::IsProfilingEnabled() {
  return profiling_enabled_;
}

double Config::GetProfileInterval() {
  return profile_interval_s_;
}

}  // namespace core
