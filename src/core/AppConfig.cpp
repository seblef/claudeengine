#include "core/AppConfig.h"

#include "core/YamlSerialiser.h"

#include <loguru.hpp>

namespace core {

GraphicsConfig    AppConfig::graphics_;
ShadowConfig      AppConfig::shadows_;
PostProcessConfig AppConfig::post_process_;
PlayerConfig      AppConfig::player_;
PhysicsConfig     AppConfig::physics_;

void GraphicsConfig::Parse(const YAML::Node& node) {
  if (node["width"])    width_    = node["width"].as<int>();
  if (node["height"])   height_   = node["height"].as<int>();
  if (node["windowed"]) windowed_ = node["windowed"].as<bool>();
  LOG_F(INFO, "Graphics config: %dx%d windowed=%s",
        width_, height_, windowed_ ? "true" : "false");
}

void AppConfig::Init(const std::filesystem::path& config_path) {
  YAML::Node root = core::LoadYamlFile(config_path);
  if (auto gfx = root["graphics"])    graphics_.Parse(gfx);
  if (auto shadows = root["shadows"]) shadows_.Parse(shadows);
  if (auto pp = root["post_process"]) post_process_.Parse(pp);
  if (auto p = root["player"])        player_.Parse(p);
  if (auto phys = root["physics"])    physics_.Parse(phys);
}

}  // namespace core
