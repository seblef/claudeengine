#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "abstract/VideoDevice.h"
#include "core/Camera.h"
#include "core/ShadowConfig.h"
#include "renderer/Light.h"
#include "renderer/ShadowMap.h"

namespace renderer {

// Pre-allocates all shadow maps at startup and assigns them to lights each frame
// based on screen-space projected size, with per-tier hysteresis to prevent
// flickering.
//
// Two independent pools are managed: one for 2D shadow maps (spot lights) and
// one for cube shadow maps (omni lights — cube-map rendering deferred to #150).
//
// Usage per frame:
//   pool.Assign(lights, camera);         // distributes slots
//   pool.GetShadowMap(light);            // nullptr → no shadow this frame
//   pool.GetAssignedResolution(light);   // 0 → not assigned
class ShadowMapPool {
 public:
  ShadowMapPool(abstract::VideoDevice* video, const core::ShadowConfig& config);

  ShadowMapPool(const ShadowMapPool&)            = delete;
  ShadowMapPool& operator=(const ShadowMapPool&) = delete;

  // Distributes pool slots among shadow-casting lights for the current frame.
  // Must be called once per frame before rendering shadow maps.
  void Assign(const std::vector<Light*>& lights, const core::Camera& camera);

  // Returns the ShadowMap assigned to this light this frame, or nullptr.
  [[nodiscard]] const ShadowMap* GetShadowMap(const Light* light) const;

  // Returns the resolution of the shadow map assigned to this light, or 0.
  [[nodiscard]] int GetAssignedResolution(const Light* light) const;

  // Releases all slot assignments (call on scene clear).
  void ClearAll();

 private:
  // One slot in a tier: a pre-allocated ShadowMap plus its current owner.
  struct Slot {
    std::unique_ptr<ShadowMap> map;
    // cppcheck-suppress unusedStructMember
    const Light*               owner              = nullptr;
    // cppcheck-suppress unusedStructMember
    float                      last_screen_radius = 0.f;
  };

  // A resolution tier: all slots share the same resolution.
  struct TierSlots {
    // cppcheck-suppress unusedStructMember
    int               resolution;
    // cppcheck-suppress unusedStructMember
    std::vector<Slot> slots;
  };

  // Allocates all ShadowMaps for one pool from the given config.
  static std::vector<TierSlots> BuildPool(abstract::VideoDevice*        video,
                                          const core::ShadowPoolConfig& cfg);

  // Returns the index into tiers of the highest-resolution tier whose screen-
  // size threshold is <= screen_radius, capped by max_resolution.
  [[nodiscard]] int FindTargetTierIdx(float                         screen_radius,
                                      int                           max_resolution,
                                      const std::vector<TierSlots>& tiers) const;

  // Runs the assign loop for one pool (2D or cube).
  // eligible: lights whose type is handled by this pool.
  void AssignPool(const std::vector<const Light*>&               eligible,
                  const std::unordered_map<const Light*, float>& radii,
                  std::vector<TierSlots>&                        pool);

  // cppcheck-suppress unusedStructMember
  core::ShadowConfig             config_;
  // cppcheck-suppress unusedStructMember
  std::vector<TierSlots>         pool_2d_;
  // cppcheck-suppress unusedStructMember
  std::vector<TierSlots>         pool_cube_;

  // Maps light → assigned ShadowMap for the current frame.
  // cppcheck-suppress unusedStructMember
  std::unordered_map<const Light*, ShadowMap*> assignments_;
};

}  // namespace renderer
