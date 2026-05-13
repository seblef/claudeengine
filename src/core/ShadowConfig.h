#pragma once

#include <unordered_map>
#include <vector>

#include <yaml-cpp/yaml.h>

namespace core {

// Configuration for one pool (2D or cube) from the shadows config section.
struct ShadowPoolConfig {
  int              slots_per_tier = 8;
  std::vector<int> tiers          = {64, 128, 256, 512, 1024};
};

// Holds shadow-map pool configuration from the [shadows] section of config.yaml.
//
// Screen-size thresholds map resolution → minimum screen-space radius (pixels).
// Lights with a projected radius below min_threshold receive no shadow this frame.
// Hysteresis prevents per-frame tier flickering: a light keeps its current tier
// until its screen radius drops below  tier_threshold * hysteresis_factor.
class ShadowConfig {
 public:
  // Populates fields from the provided YAML node (the "shadows" subtree).
  void Parse(const YAML::Node& node);

  [[nodiscard]] const ShadowPoolConfig& Get2DPool()   const { return pool_2d_; }
  [[nodiscard]] const ShadowPoolConfig& GetCubePool() const { return pool_cube_; }

  // Returns the minimum screen-space radius (pixels) required to get a shadow
  // map at the given resolution.  Returns INT_MAX when resolution is unknown.
  [[nodiscard]] int GetScreenSizeThreshold(int resolution) const;

  // Lights below this screen-space radius (pixels) get no shadow this frame.
  [[nodiscard]] int   GetMinThreshold()     const { return min_threshold_; }

  // Fraction of a tier's threshold a light must drop below before it is evicted
  // from that tier.  Range (0, 1]; lower values mean stickier assignments.
  [[nodiscard]] float GetHysteresisFactor() const { return hysteresis_factor_; }

 private:
  // cppcheck-suppress unusedStructMember
  ShadowPoolConfig            pool_2d_;
  // cppcheck-suppress unusedStructMember
  ShadowPoolConfig            pool_cube_;
  // cppcheck-suppress unusedStructMember
  std::unordered_map<int,int> screen_size_thresholds_;
  // cppcheck-suppress unusedStructMember
  int                         min_threshold_     = 10;
  // cppcheck-suppress unusedStructMember
  float                       hysteresis_factor_ = 0.8f;
};

}  // namespace core
