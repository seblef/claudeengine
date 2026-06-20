#pragma once

#include <filesystem>

#include <yaml-cpp/yaml.h>

namespace core {

// Holds post-processing configuration from the [post_process] section of config.yaml.
class PostProcessConfig {
 public:
  // Populates fields from the provided YAML node (the "post_process" subtree).
  void Parse(const YAML::Node& node);

  // Persists the eye-adaptation float parameters to the post_process section of
  // config.yaml.  The file is read, the three keys are updated, and the file is
  // written back so all other sections are preserved.
  void Save(const std::filesystem::path& config_path,
            float eye_key, float eye_min_exposure, float eye_max_exposure);

  [[nodiscard]] bool  IsBloomEnabled()         const { return bloom_; }
  [[nodiscard]] bool  IsEyeAdaptationEnabled() const { return eye_adaptation_; }
  [[nodiscard]] float GetEyeKey()              const { return eye_key_; }
  [[nodiscard]] float GetEyeMinExposure()      const { return eye_min_exposure_; }
  [[nodiscard]] float GetEyeMaxExposure()      const { return eye_max_exposure_; }

 private:
  bool  bloom_            = true;
  bool  eye_adaptation_   = true;
  float eye_key_          = 0.18f;
  float eye_min_exposure_ = 0.1f;
  float eye_max_exposure_ = 0.18f;
};

}  // namespace core
