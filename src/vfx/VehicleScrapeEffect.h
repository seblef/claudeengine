#pragma once

#include "game/IVehicleScrapeListener.h"
#include "physics/VehicleDesc.h"

namespace audio {
class ResourceManager;
class SoundManager;
}  // namespace audio

namespace vfx {

class VFXScrape;

// Drives a continuous vfx::VFXScrape effect (sparks + looping screech) from
// the per-physics-step sustained-contact events GameVehicle forwards through
// game::IVehicleScrapeListener.
//
// Unlike VehicleCrashSound/VehicleDamage (fed by one-shot OnCollision impacts,
// and owned directly by GameVehicle since they stay within game/), this class
// lives in vfx/ and is wired to a GameVehicle externally via
// GameVehicle::SetScrapeListener() — game/ must not depend on vfx/, since
// vfx/ already depends on game/, so the trigger can't be owned by GameVehicle
// itself. See src/game/IVehicleScrapeListener.h for the rationale.
//
// A short grace period absorbs single-step gaps in the contact stream (solver
// jitter) so the effect does not flicker on and off; the effect stops once
// contact has genuinely lapsed for longer than desc.contact_grace_time, or
// immediately when the sliding speed drops below desc.min_speed.
class VehicleScrapeEffect : public game::IVehicleScrapeListener {
 public:
  VehicleScrapeEffect(const physics::ScrapeDesc& desc,
                      audio::SoundManager* sound_manager,
                      audio::ResourceManager* resource_manager);

  VehicleScrapeEffect(const VehicleScrapeEffect&)            = delete;
  VehicleScrapeEffect& operator=(const VehicleScrapeEffect&) = delete;

  // Stops any effect still running.
  ~VehicleScrapeEffect() override;

  void Update(float dt) override;

  void RegisterContact(const core::Vec3f& world_point,
                       const core::Vec3f& world_normal,
                       const core::Vec3f& relative_velocity) override;

  // True while a vfx::VFXScrape is currently active. Exposed for testing.
  [[nodiscard]] bool IsActive() const { return active_ != nullptr; }

 private:
  void Stop();

  // cppcheck-suppress unusedStructMember
  physics::ScrapeDesc     desc_;
  // cppcheck-suppress unusedStructMember
  audio::SoundManager*    sound_manager_;
  // cppcheck-suppress unusedStructMember
  audio::ResourceManager* resource_manager_;

  // Non-owning; owned by vfx::VFXSystem. Null when no scrape is active.
  // cppcheck-suppress unusedStructMember
  VFXScrape* active_ = nullptr;
  // cppcheck-suppress unusedStructMember
  float      time_since_contact_ = 0.f;
};

}  // namespace vfx
