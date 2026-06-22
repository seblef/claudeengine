#include "core/PostProcessConfig.h"

#include <fstream>

#include <loguru.hpp>

#include "core/YamlSerialiser.h"

namespace core {

void PostProcessConfig::Parse(const YAML::Node& node) {
  if (node["bloom"])            bloom_          = node["bloom"].as<bool>();
  if (node["eye_adaptation"])   eye_adaptation_ = node["eye_adaptation"].as<bool>();
  if (node["eye_key"])          eye_key_         = node["eye_key"].as<float>();
  if (node["eye_min_exposure"]) eye_min_exposure_= node["eye_min_exposure"].as<float>();
  if (node["eye_max_exposure"]) eye_max_exposure_= node["eye_max_exposure"].as<float>();
  LOG_F(INFO, "Post-process config: bloom=%s eye_adaptation=%s "
        "eye_key=%.3f eye_min_exposure=%.3f eye_max_exposure=%.3f",
        bloom_ ? "true" : "false", eye_adaptation_ ? "true" : "false",
        eye_key_, eye_min_exposure_, eye_max_exposure_);
}

void PostProcessConfig::Save(const std::filesystem::path& config_path,
                              float eye_key, float eye_min_exposure,
                              float eye_max_exposure) {
  YAML::Node root;
  try {
    root = core::LoadYamlFile(config_path);
  } catch (...) {}

  root["post_process"]["eye_key"]          = eye_key;
  root["post_process"]["eye_min_exposure"] = eye_min_exposure;
  root["post_process"]["eye_max_exposure"] = eye_max_exposure;

  std::ofstream out(config_path);
  out << root;

  eye_key_          = eye_key;
  eye_min_exposure_ = eye_min_exposure;
  eye_max_exposure_ = eye_max_exposure;
  LOG_F(INFO, "Post-process config saved: eye_key=%.3f eye_min_exposure=%.3f "
        "eye_max_exposure=%.3f", eye_key_, eye_min_exposure_, eye_max_exposure_);
}

}  // namespace core
