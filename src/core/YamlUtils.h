#pragma once

#include <filesystem>

#include <yaml-cpp/yaml.h>

namespace core {

// Loads and parses a YAML file. Throws std::runtime_error on failure.
YAML::Node LoadYamlFile(const std::filesystem::path& path);

}  // namespace core
