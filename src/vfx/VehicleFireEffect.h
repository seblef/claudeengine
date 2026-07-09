#pragma once

#include "core/Mat4f.h"
#include "game/DamageZone.h"
#include "game/IVehicleDamageListener.h"
#include "game/IVehicleFireListener.h"
#include "physics/VehicleDesc.h"

namespace game {
class VehicleDamage;
}  // namespace game

namespace particles {
class ParticleSystemTemplate;
}  // namespace particles

namespace vfx {

class VFXFire;

// Drives a continuous vfx::VFXFire effect (looping fire + smoke) from a
// vehicle's damage state: starts the moment any zone's damage fraction
// crosses desc.damage_threshold (game::IVehicleDamageListener), follows the
// vehicle body every frame (game::IVehicleFireListener), and stops once
// desc.min_burn_time has elapsed since it started AND either the vehicle is
// wrecked (its last configured damage threshold, conventionally 1.0 —
// superseded by the wreck explosion) or every zone's damage fraction has
// dropped back below desc.damage_threshold (e.g. after a repair).
//
// The min_burn_time gate exists because a single hard impact (or a quick
// burst of impacts) can cross both the 80% and 100% thresholds within the
// same frame or two — without it the fire would start and immediately stop
// again, which reads as broken since no wreck explosion exists yet to
// visually take over.
//
// Lives in vfx/ and is wired to a GameVehicle externally via
// GameVehicle::SetFireListener() plus VehicleDamage::AddListener() — game/
// must not depend on vfx/, since vfx/ already depends on game/, so the
// trigger can't be owned by GameVehicle itself. See
// src/game/IVehicleFireListener.h for the rationale.
//
// Loads the fire particle template once (if a Renderer is instanced) and
// holds it for this object's whole lifetime, for the same reason
// VehicleScrapeEffect holds its spark template — see VehicleScrapeEffect.h.
class VehicleFireEffect : public game::IVehicleDamageListener,
                          public game::IVehicleFireListener {
 public:
  // damage is non-owning and must outlive this VehicleFireEffect; polled
  // every frame to check whether every zone's damage fraction has dropped
  // back below desc.damage_threshold (e.g. after a repair) — a downward
  // crossing that game::IVehicleDamageListener does not itself report, since
  // it only fires on upward threshold crossings.
  VehicleFireEffect(const physics::FireDesc& desc, const game::VehicleDamage& damage);

  VehicleFireEffect(const VehicleFireEffect&)            = delete;
  VehicleFireEffect& operator=(const VehicleFireEffect&) = delete;

  // Stops any effect still running.
  ~VehicleFireEffect() override;

  // game::IVehicleFireListener
  void OnVehicleTransformUpdated(float dt, const core::Mat4f& world_transform) override;

  // game::IVehicleDamageListener
  void OnDamageThresholdCrossed(game::DamageZone zone, float threshold,
                                float fraction) override;

  // True while a vfx::VFXFire is currently active. Exposed for testing.
  [[nodiscard]] bool IsActive() const { return active_ != nullptr; }

 private:
  void Start(const core::Mat4f& world_transform);
  void Stop();

  // True once every zone's damage fraction has dropped below
  // desc_.damage_threshold.
  [[nodiscard]] bool AllZonesBelowThreshold() const;

  // cppcheck-suppress unusedStructMember
  physics::FireDesc desc_;
  // Non-owning; see the constructor's doc comment.
  // cppcheck-suppress unusedStructMember
  const game::VehicleDamage* damage_;

  // AddRef'd for this object's whole lifetime; Release()d in the destructor.
  // Null if no Renderer was instanced at construction time.
  // cppcheck-suppress unusedStructMember
  particles::ParticleSystemTemplate* fire_template_ = nullptr;

  // Non-owning; owned by vfx::VFXSystem. Null when no fire is active.
  // cppcheck-suppress unusedStructMember
  VFXFire* active_ = nullptr;

  // Latest transform seen via OnVehicleTransformUpdated(); used to place the
  // effect if a threshold crossing is notified before the first frame tick.
  // cppcheck-suppress unusedStructMember
  core::Mat4f last_transform_ = core::Mat4f::kIdentity;

  // Seconds elapsed since Start(); reset on every Start(). Gates how soon a
  // pending stop (wreck or repair) can actually take effect.
  // cppcheck-suppress unusedStructMember
  float active_time_ = 0.f;

  // Latched true once the wreck threshold is crossed while active; consumed
  // (stops the fire) once active_time_ reaches desc_.min_burn_time.
  // cppcheck-suppress unusedStructMember
  bool wreck_pending_ = false;
};

}  // namespace vfx
