#include "core/PostProcessConfig.h"

#include <loguru.hpp>

namespace core {

void PostProcessConfig::Parse(const YAML::Node& node) {
  if (node["bloom"])          bloom_          = node["bloom"].as<bool>();
  if (node["eye_adaptation"]) eye_adaptation_ = node["eye_adaptation"].as<bool>();
  LOG_F(INFO, "Post-process config: bloom=%s eye_adaptation=%s",
        bloom_ ? "true" : "false", eye_adaptation_ ? "true" : "false");
}

}  // namespace core
