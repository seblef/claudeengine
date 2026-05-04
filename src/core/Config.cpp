#include "core/Config.h"

#include <string_view>

namespace core {

std::filesystem::path Config::data_folder_ = ".";

void Config::Init(int argc, char* argv[]) {
  for (int i = 1; i < argc - 1; ++i) {
    if (std::string_view(argv[i]) == "--data-path") {
      data_folder_ = argv[i + 1];
      return;
    }
  }
}

const std::filesystem::path& Config::GetDataFolder() {
  return data_folder_;
}

}  // namespace core
