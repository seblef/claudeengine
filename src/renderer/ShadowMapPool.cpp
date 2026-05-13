#include "renderer/ShadowMapPool.h"

#include <algorithm>
#include <cmath>

namespace renderer {

std::vector<ShadowMapPool::TierSlots> ShadowMapPool::BuildPool(
    abstract::VideoDevice*        video,
    const core::ShadowPoolConfig& cfg) {
  std::vector<TierSlots> pool;
  pool.reserve(cfg.tiers.size());
  for (const int res : cfg.tiers) {
    TierSlots tier;
    tier.resolution = res;
    tier.slots.resize(cfg.slots_per_tier);
    for (auto& slot : tier.slots)
      slot.map = std::make_unique<ShadowMap>(video, res);
    pool.push_back(std::move(tier));
  }
  return pool;
}

ShadowMapPool::ShadowMapPool(abstract::VideoDevice*    video,
                             const core::ShadowConfig& config)
    : config_(config),
      pool_2d_(BuildPool(video, config.Get2DPool())),
      pool_cube_(BuildPool(video, config.GetCubePool())) {}

void ShadowMapPool::Assign(const std::vector<Light*>& lights,
                           const core::Camera&         camera) {
  // Pre-compute camera-derived values shared across all lights this frame.
  const core::Vec3f eye_pos           = camera.GetPosition();
  const float       half_screen_height = camera.GetScreenCenter().y;
  const float       tan_half_fov       = std::tan(camera.GetFOV() * 0.5f);

  // Compute screen-space radius for every shadow-casting local light.
  std::unordered_map<const Light*, float> radii;
  std::vector<const Light*> eligible_2d;

  for (const Light* light : lights) {
    if (!light->GetCastShadow()) continue;
    const LightType type = light->GetType();
    if (type == LightType::kGlobal) continue;  // CSM handled separately

    const float radius = light->ComputeScreenRadius(eye_pos, half_screen_height,
                                                    tan_half_fov);
    radii[light] = radius;

    if (type == LightType::kCircleSpot || type == LightType::kRectSpot)
      eligible_2d.push_back(light);
    // kOmni → cube pool (deferred to #150; pre-allocate but no assignment yet)
  }

  assignments_.clear();
  AssignPool(eligible_2d, radii, pool_2d_);
}

void ShadowMapPool::AssignPool(const std::vector<const Light*>&              eligible,
                                const std::unordered_map<const Light*, float>& radii,
                                std::vector<TierSlots>&                        pool) {
  const int min_px = config_.GetMinThreshold();
  const float hyst = config_.GetHysteresisFactor();

  // Step 1: apply hysteresis — retained lights keep their current slot.
  std::unordered_set<const Light*> retained;
  for (auto& tier : pool) {
    const float tier_threshold = static_cast<float>(
        config_.GetScreenSizeThreshold(tier.resolution));
    for (auto& slot : tier.slots) {
      if (!slot.owner) continue;
      const auto it  = radii.find(slot.owner);
      const float r  = (it != radii.end()) ? it->second : 0.f;
      const bool keep = r >= static_cast<float>(min_px) &&
                        r >= tier_threshold * hyst;
      if (keep) {
        retained.insert(slot.owner);
        slot.last_screen_radius = r;
        assignments_[slot.owner] = slot.map.get();
      } else {
        slot.owner              = nullptr;
        slot.last_screen_radius = 0.f;
      }
    }
  }

  // Step 2: collect lights that still need a slot (not retained, above min).
  std::vector<std::pair<const Light*, float>> to_assign;
  to_assign.reserve(eligible.size());
  for (const Light* light : eligible) {
    if (retained.count(light)) continue;
    const auto it = radii.find(light);
    if (it == radii.end()) continue;
    if (it->second >= static_cast<float>(min_px))
      to_assign.emplace_back(light, it->second);
  }

  // Step 3: sort by screen radius descending (largest → highest priority).
  std::sort(to_assign.begin(), to_assign.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  // Step 4: greedy assignment — target tier, fall back to smaller tiers.
  for (const auto& [light, radius] : to_assign) {
    const int target = FindTargetTierIdx(radius, light->GetShadowResolution(), pool);
    bool assigned = false;
    for (int t = target; t >= 0 && !assigned; --t) {
      auto slot_it = std::find_if(pool[t].slots.begin(), pool[t].slots.end(),
                                  [](const Slot& s) { return !s.owner; });
      if (slot_it != pool[t].slots.end()) {
        slot_it->owner              = light;
        slot_it->last_screen_radius = radius;
        assignments_[light]         = slot_it->map.get();
        assigned                    = true;
      }
    }
  }
}

int ShadowMapPool::FindTargetTierIdx(float                         screen_radius,
                                      int                           max_resolution,
                                      const std::vector<TierSlots>& tiers) const {
  int result = 0;
  for (int i = 0; i < static_cast<int>(tiers.size()); ++i) {
    const int res = tiers[i].resolution;
    if (res > max_resolution) break;
    if (static_cast<float>(config_.GetScreenSizeThreshold(res)) <= screen_radius)
      result = i;
  }
  return result;
}

const ShadowMap* ShadowMapPool::GetShadowMap(const Light* light) const {
  const auto it = assignments_.find(light);
  return (it != assignments_.end()) ? it->second : nullptr;
}

int ShadowMapPool::GetAssignedResolution(const Light* light) const {
  const ShadowMap* smap = GetShadowMap(light);
  return smap ? smap->GetResolution() : 0;
}

void ShadowMapPool::ClearAll() {
  for (auto& tier : pool_2d_)
    for (auto& slot : tier.slots) {
      slot.owner = nullptr;
      slot.last_screen_radius = 0.f;
    }
  for (auto& tier : pool_cube_)
    for (auto& slot : tier.slots) {
      slot.owner = nullptr;
      slot.last_screen_radius = 0.f;
    }
  assignments_.clear();
}

}  // namespace renderer
