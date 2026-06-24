#pragma once

#include <string>

#include <yaml-cpp/yaml.h>

namespace core {

/// Holds physics-related configuration from the [physics] section of config.yaml.
class PhysicsConfig {
 public:
  /// Populates fields from the provided YAML node (the "physics" subtree).
  void Parse(const YAML::Node& node);

  /// Directory where serialized Jolt shape cache files are stored.
  /// Empty string means disk caching is disabled.
  [[nodiscard]] const std::string& GetShapeCacheDir() const {
    return shape_cache_dir_;
  }

 private:
  std::string shape_cache_dir_;
};

}  // namespace core
