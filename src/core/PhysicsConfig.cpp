#include "core/PhysicsConfig.h"

#include <loguru.hpp>

namespace core {

void PhysicsConfig::Parse(const YAML::Node& node) {
  if (node["shape_cache_dir"])
    shape_cache_dir_ = node["shape_cache_dir"].as<std::string>();
  if (!shape_cache_dir_.empty())
    LOG_F(INFO, "Physics shape cache dir: %s", shape_cache_dir_.c_str());
  else
    LOG_F(INFO, "Physics shape cache dir: (disabled)");
}

}  // namespace core
