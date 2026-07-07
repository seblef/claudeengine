#pragma once

#include "core/Vec3f.h"
#include "game/SoundEffectComponent.h"
#include "physics/VehicleDesc.h"

namespace audio {
class ResourceManager;
class SoundManager;
}  // namespace audio

namespace game {

// Plays impulse-scaled, positional crash sounds on vehicle impact.
//
// GameVehicle forwards every raw collision here (regardless of whether the
// hit is enough to damage a zone). Impacts are classified into a light /
// medium / heavy severity tier by raw impulse magnitude, the matching
// SoundEffectComponent is triggered with gain scaled by impulse, and a
// debounce cooldown suppresses further triggers so a single sustained
// contact does not spam overlapping one-shots.
//
// Fully GameObject-free: callers are responsible for providing the
// world-space contact point. Safe to construct with null audio managers
// (becomes a silent no-op, matching SoundEffectComponent's own contract).
class VehicleCrashSound {
 public:
  enum class Tier { kNone, kLight, kMedium, kHeavy };

  VehicleCrashSound(const physics::CrashSoundDesc& desc,
                    audio::SoundManager* sound_manager,
                    audio::ResourceManager* resource_manager);

  VehicleCrashSound(const VehicleCrashSound&)            = delete;
  VehicleCrashSound& operator=(const VehicleCrashSound&) = delete;

  // Advances the debounce cooldown. Call once per frame.
  void Update(float dt);

  // Classifies impulse into a severity tier and, if it clears both the
  // minimum-impulse floor and the debounce cooldown, triggers the matching
  // one-shot at world_point (gain scaled by impulse magnitude) and resets
  // the cooldown. No-op below desc.min_impulse or while debounced.
  void RegisterImpact(const core::Vec3f& world_point, float impulse);

  // Classifies impulse into a severity tier per desc's ascending thresholds.
  // Returns Tier::kNone below desc.min_impulse. Exposed for unit testing.
  [[nodiscard]] static Tier ClassifyTier(float impulse,
                                        const physics::CrashSoundDesc& desc);

  // Normalises impulse into a [0, 1] gain multiplier, saturating at
  // desc.max_impulse. Exposed for unit testing.
  [[nodiscard]] static float ComputeGain(float impulse,
                                        const physics::CrashSoundDesc& desc);

  // True while a prior impact's debounce cooldown is still active.
  [[nodiscard]] bool IsDebounced() const { return cooldown_remaining_ > 0.f; }

 private:
  // cppcheck-suppress unusedStructMember
  physics::CrashSoundDesc desc_;

  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate ; initialized in VehicleCrashSound.cpp,
  // not visible to cppcheck when this header is checked from a TU that only
  // declares (not defines) the constructor, e.g. the unit test file.
  SoundEffectComponent light_;
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate
  SoundEffectComponent medium_;
  // cppcheck-suppress unusedStructMember
  // cppcheck-suppress uninitMemberVarPrivate
  SoundEffectComponent heavy_;

  // cppcheck-suppress unusedStructMember
  float cooldown_remaining_ = 0.f;
};

}  // namespace game
