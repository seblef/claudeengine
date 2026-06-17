#include "core/PlayerConfig.h"

#include <loguru.hpp>

namespace core {

void PlayerConfig::Parse(const YAML::Node& node) {
  if (node["capsule_radius"])      capsule_radius_      = node["capsule_radius"].as<float>();
  if (node["capsule_half_height"]) capsule_half_height_ = node["capsule_half_height"].as<float>();
  if (node["eye_offset"])          eye_offset_          = node["eye_offset"].as<float>();
  if (node["move_speed"])          move_speed_          = node["move_speed"].as<float>();
  if (node["mouse_sensitivity"])   mouse_sensitivity_   = node["mouse_sensitivity"].as<float>();
  if (node["jump_speed"])          jump_speed_          = node["jump_speed"].as<float>();
  if (node["gravity"])             gravity_             = node["gravity"].as<float>();

  LOG_F(INFO, "Player config: capsule r=%.2f h=%.2f eye=%.2f speed=%.1f "
              "mouse=%.4f jump=%.1f gravity=%.2f",
        capsule_radius_, capsule_half_height_, eye_offset_,
        move_speed_, mouse_sensitivity_, jump_speed_, gravity_);
}

}  // namespace core
