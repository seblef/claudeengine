#pragma once

#include <yaml-cpp/yaml.h>

namespace core {

// Holds post-processing feature flags from the [post_process] section of config.yaml.
class PostProcessConfig {
 public:
  // Populates fields from the provided YAML node (the "post_process" subtree).
  void Parse(const YAML::Node& node);

  [[nodiscard]] bool IsBloomEnabled()         const { return bloom_; }
  [[nodiscard]] bool IsEyeAdaptationEnabled() const { return eye_adaptation_; }

 private:
  bool bloom_          = true;
  bool eye_adaptation_ = true;
};

}  // namespace core
