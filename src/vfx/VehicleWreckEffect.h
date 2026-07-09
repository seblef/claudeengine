#pragma once

#include "core/Vec3f.h"
#include "game/IVehicleWreckListener.h"
#include "game/SoundEffectComponent.h"
#include "physics/VehicleDesc.h"

namespace audio {
class ResourceManager;
class SoundManager;
}  // namespace audio

namespace particles {
class ParticleSystemTemplate;
}  // namespace particles

namespace vfx {

// Drives the one-shot wreck payoff from a vehicle's terminal damage
// transition: game::GameVehicle calls game::IVehicleWreckListener::
// OnVehicleWrecked() exactly once, the instant any zone's damage fraction
// first reaches the wreck threshold. This spawns a vfx::VFXExplosion and
// triggers a wreck sound one-shot at the vehicle's position. Further calls
// are ignored — GameVehicle already guarantees OnVehicleWrecked() fires at
// most once per vehicle, but the guard is kept here too since this is the
// class responsible for not double-playing the payoff.
//
// Lives in vfx/ and is wired to a GameVehicle externally via
// GameVehicle::SetWreckListener() — game/ must not depend on vfx/, since
// vfx/ already depends on game/, so the trigger can't be owned by
// GameVehicle itself. See src/game/IVehicleWreckListener.h for the
// rationale.
//
// Loads the explosion particle template once (if a Renderer is instanced)
// and holds it for this object's whole lifetime, for the same reason
// VehicleFireEffect holds its fire template — see VehicleFireEffect.h.
class VehicleWreckEffect : public game::IVehicleWreckListener {
 public:
  // sound_manager and resource_manager may be null (the wreck sound becomes
  // silent, matching SoundEffectComponent's own contract).
  VehicleWreckEffect(const physics::WreckDesc& desc,
                     audio::SoundManager* sound_manager,
                     audio::ResourceManager* resource_manager);

  VehicleWreckEffect(const VehicleWreckEffect&)            = delete;
  VehicleWreckEffect& operator=(const VehicleWreckEffect&) = delete;

  ~VehicleWreckEffect() override;

  // game::IVehicleWreckListener
  void OnVehicleWrecked(const core::Vec3f& world_position) override;

  // True once the wreck payoff has fired. Exposed for testing.
  [[nodiscard]] bool HasTriggered() const { return triggered_; }

 private:
  // cppcheck-suppress unusedStructMember
  physics::WreckDesc desc_;

  // cppcheck-suppress unusedStructMember
  game::SoundEffectComponent wreck_sound_;

  // AddRef'd for this object's whole lifetime; Release()d in the destructor.
  // Null if no Renderer was instanced at construction time.
  // cppcheck-suppress unusedStructMember
  particles::ParticleSystemTemplate* explosion_template_ = nullptr;

  // cppcheck-suppress unusedStructMember
  bool triggered_ = false;
};

}  // namespace vfx
