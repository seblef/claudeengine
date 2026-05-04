#include "core/YamlUtils.h"

#include <stdexcept>

namespace core {

YAML::Node LoadYamlFile(const std::filesystem::path& path) {
  try {
    return YAML::LoadFile(path.string());
  } catch (const YAML::Exception& e) {
    throw std::runtime_error("Failed to load YAML '" + path.string() + "': " + e.what());
  }
}

}  // namespace core
