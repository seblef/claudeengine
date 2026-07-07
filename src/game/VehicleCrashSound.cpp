#include "game/VehicleCrashSound.h"

#include <algorithm>

namespace game {

VehicleCrashSound::VehicleCrashSound(const physics::CrashSoundDesc& desc,
                                     audio::SoundManager* sound_manager,
                                     audio::ResourceManager* resource_manager)
    : desc_(desc),
      light_(desc.light_sound, sound_manager, resource_manager, /*priority=*/0),
      medium_(desc.medium_sound, sound_manager, resource_manager, /*priority=*/1),
      heavy_(desc.heavy_sound, sound_manager, resource_manager, /*priority=*/2) {}

void VehicleCrashSound::Update(float dt) {
  cooldown_remaining_ = std::max(0.f, cooldown_remaining_ - dt);
}

void VehicleCrashSound::RegisterImpact(const core::Vec3f& world_point, float impulse) {
  if (IsDebounced()) return;

  const Tier tier = ClassifyTier(impulse, desc_);
  if (tier == Tier::kNone) return;

  SoundEffectComponent* sfx = nullptr;
  switch (tier) {
    case Tier::kLight:  sfx = &light_;  break;
    case Tier::kMedium: sfx = &medium_; break;
    case Tier::kHeavy:  sfx = &heavy_;  break;
    case Tier::kNone:   return;
  }

  sfx->Trigger(world_point, ComputeGain(impulse, desc_));
  cooldown_remaining_ = desc_.debounce_time;
}

// static
VehicleCrashSound::Tier VehicleCrashSound::ClassifyTier(
    float impulse, const physics::CrashSoundDesc& desc) {
  if (impulse < desc.min_impulse) return Tier::kNone;
  if (impulse >= desc.heavy_impulse) return Tier::kHeavy;
  if (impulse >= desc.medium_impulse) return Tier::kMedium;
  return Tier::kLight;
}

// static
float VehicleCrashSound::ComputeGain(float impulse,
                                     const physics::CrashSoundDesc& desc) {
  if (desc.max_impulse <= 0.f) return 1.f;
  return std::clamp(impulse / desc.max_impulse, 0.f, 1.f);
}

}  // namespace game
