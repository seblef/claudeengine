#include "core/ShadowConfig.h"

#include <climits>

#include <loguru.hpp>

namespace core {

namespace {

void ParsePoolConfig(const YAML::Node& node, ShadowPoolConfig& out) {
  if (node["slots_per_tier"]) out.slots_per_tier = node["slots_per_tier"].as<int>();
  if (node["tiers"])          out.tiers           = node["tiers"].as<std::vector<int>>();
}

}  // namespace

void ShadowConfig::Parse(const YAML::Node& node) {
  if (node["2d_pool"])   ParsePoolConfig(node["2d_pool"],   pool_2d_);
  if (node["cube_pool"]) ParsePoolConfig(node["cube_pool"], pool_cube_);

  if (node["screen_size_thresholds"]) {
    const YAML::Node& t = node["screen_size_thresholds"];
    // Keys are "t<resolution>" for tier thresholds and "min" for the cutoff.
    for (const auto& kv : t) {
      const std::string key = kv.first.as<std::string>();
      if (key == "min") {
        min_threshold_ = kv.second.as<int>();
      } else if (!key.empty() && key[0] == 't') {
        const int res = std::stoi(key.substr(1));
        screen_size_thresholds_[res] = kv.second.as<int>();
      }
    }
  }

  if (node["hysteresis_factor"])
    hysteresis_factor_ = node["hysteresis_factor"].as<float>();

  LOG_F(INFO, "Shadow config: 2d_pool slots=%d tiers=%zu, cube_pool slots=%d tiers=%zu, "
              "min_threshold=%d, hysteresis=%.2f",
        pool_2d_.slots_per_tier, pool_2d_.tiers.size(),
        pool_cube_.slots_per_tier, pool_cube_.tiers.size(),
        min_threshold_, hysteresis_factor_);
}

int ShadowConfig::GetScreenSizeThreshold(int resolution) const {
  const auto it = screen_size_thresholds_.find(resolution);
  return (it != screen_size_thresholds_.end()) ? it->second : INT_MAX;
}

}  // namespace core
