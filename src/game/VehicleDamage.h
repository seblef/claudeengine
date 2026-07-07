#pragma once

#include <array>
#include <vector>

#include "core/Vec3f.h"
#include "game/DamageZone.h"
#include "physics/VehicleDesc.h"

namespace game {

class IVehicleDamageListener;

// Progressive per-zone damage model for a vehicle: tracks HP for Front, Rear,
// Left, Right and Roof independently, converts collision impulse into
// damage, and notifies listeners when a zone's damage fraction crosses one of
// its configured thresholds.
//
// This is the single authoritative source of vehicle damage state — other
// systems (steering penalty, engine slowdown, smoke, wreck, mesh swap) must
// subscribe via AddListener() rather than inspecting collisions themselves.
//
// Fully Jolt-free and GameObject-free: it knows nothing about scene transforms
// or physics bodies. Callers (GameVehicle) are responsible for converting a
// world-space impact point into vehicle body-local space before calling
// RegisterImpact().
class VehicleDamage {
 public:
  explicit VehicleDamage(const physics::VehicleDesc& desc);

  VehicleDamage(const VehicleDamage&)            = delete;
  VehicleDamage& operator=(const VehicleDamage&) = delete;

  // Classifies local_point (vehicle body-local space) into a zone, applies
  // impulse * impulse_to_damage_scale as damage to that zone (clamped to
  // [0, max_hp]), and notifies listeners for every threshold newly crossed.
  // No-op when impulse <= 0.
  void RegisterImpact(const core::Vec3f& local_point, float impulse);

  [[nodiscard]] float GetHP(DamageZone zone) const;
  [[nodiscard]] float GetMaxHP(DamageZone zone) const;
  // Damage fraction in [0, 1]: 0 = pristine, 1 = destroyed.
  [[nodiscard]] float GetDamageFraction(DamageZone zone) const;

  // Both non-owning; the listener must outlive this VehicleDamage or call
  // RemoveListener() first. Adding the same listener twice is a no-op.
  void AddListener(IVehicleDamageListener* listener);
  void RemoveListener(IVehicleDamageListener* listener);

 private:
  // Determines which zone local_point falls in, based on which axis
  // (normalised by half_extents_) has the largest magnitude. A negative-Y
  // (underside) dominant point falls back to the largest of X/Z so every
  // impact maps to one of the five zones.
  [[nodiscard]] DamageZone ClassifyZone(const core::Vec3f& local_point) const;

  static constexpr int kZoneCount = 5;

  // cppcheck-suppress unusedStructMember
  core::Vec3f half_extents_;
  // cppcheck-suppress unusedStructMember
  std::array<float, kZoneCount> max_hp_;
  // cppcheck-suppress unusedStructMember
  std::array<float, kZoneCount> hp_;
  // cppcheck-suppress unusedStructMember
  std::array<float, 4> thresholds_;
  // cppcheck-suppress unusedStructMember
  float impulse_to_damage_scale_;
  // cppcheck-suppress unusedStructMember
  std::vector<IVehicleDamageListener*> listeners_;
};

}  // namespace game
