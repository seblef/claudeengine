#include "game/VehicleDamage.h"

#include <algorithm>
#include <cmath>

#include <loguru.hpp>

#include "game/IVehicleDamageListener.h"

namespace game {

namespace {

const char* ZoneName(DamageZone zone) {
  switch (zone) {
    case DamageZone::kFront: return "Front";
    case DamageZone::kRear:  return "Rear";
    case DamageZone::kLeft:  return "Left";
    case DamageZone::kRight: return "Right";
    case DamageZone::kRoof:  return "Roof";
  }
  return "Unknown";
}

}  // namespace

VehicleDamage::VehicleDamage(const physics::VehicleDesc& desc)
    : half_extents_(desc.half_extents),
      max_hp_(desc.damage.zone_max_hp),
      hp_(desc.damage.zone_max_hp),
      thresholds_(desc.damage.thresholds),
      impulse_to_damage_scale_(desc.damage.impulse_to_damage_scale) {}

void VehicleDamage::RegisterImpact(const core::Vec3f& local_point, float impulse) {
  if (impulse <= 0.f) return;

  const DamageZone zone = ClassifyZone(local_point);
  const int        idx  = static_cast<int>(zone);

  const float old_fraction = GetDamageFraction(zone);
  const float damage       = impulse * impulse_to_damage_scale_;
  hp_[idx] = std::clamp(hp_[idx] - damage, 0.f, max_hp_[idx]);
  const float new_fraction = GetDamageFraction(zone);

  LOG_F(INFO, "VehicleDamage: %s hit for %.1f (impulse=%.1f) -> HP %.1f/%.1f (%.0f%% damaged) | "
              "Front=%.0f%% Rear=%.0f%% Left=%.0f%% Right=%.0f%% Roof=%.0f%%",
        ZoneName(zone), damage, impulse, hp_[idx], max_hp_[idx], new_fraction * 100.f,
        GetDamageFraction(DamageZone::kFront) * 100.f,
        GetDamageFraction(DamageZone::kRear)  * 100.f,
        GetDamageFraction(DamageZone::kLeft)  * 100.f,
        GetDamageFraction(DamageZone::kRight) * 100.f,
        GetDamageFraction(DamageZone::kRoof)  * 100.f);

  for (const float threshold : thresholds_) {
    if (old_fraction < threshold && new_fraction >= threshold) {
      LOG_F(WARNING, "VehicleDamage: %s crossed %.0f%% damage threshold (now %.0f%%)",
            ZoneName(zone), threshold * 100.f, new_fraction * 100.f);
      for (IVehicleDamageListener* listener : listeners_)
        listener->OnDamageThresholdCrossed(zone, threshold, new_fraction);
    }
  }
}

DamageZone VehicleDamage::ClassifyZone(const core::Vec3f& local_point) const {
  const float nx = local_point.x / half_extents_.x;
  const float ny = local_point.y / half_extents_.y;
  const float nz = local_point.z / half_extents_.z;

  // Roof only wins when the impact comes from above; an underside (negative
  // ny) impact falls through to whichever of X/Z is dominant instead, so
  // every impact still resolves to one of the five zones.
  if (ny > std::fabs(nx) && ny > std::fabs(nz)) return DamageZone::kRoof;
  if (std::fabs(nx) > std::fabs(nz)) return nx > 0.f ? DamageZone::kLeft : DamageZone::kRight;
  return nz > 0.f ? DamageZone::kFront : DamageZone::kRear;
}

float VehicleDamage::GetHP(DamageZone zone) const {
  return hp_[static_cast<int>(zone)];
}

float VehicleDamage::GetMaxHP(DamageZone zone) const {
  return max_hp_[static_cast<int>(zone)];
}

float VehicleDamage::GetDamageFraction(DamageZone zone) const {
  const int   idx = static_cast<int>(zone);
  const float max = max_hp_[idx];
  return max > 0.f ? 1.f - hp_[idx] / max : 1.f;
}

void VehicleDamage::AddListener(IVehicleDamageListener* listener) {
  if (std::find(listeners_.begin(), listeners_.end(), listener) == listeners_.end())
    listeners_.push_back(listener);
}

void VehicleDamage::RemoveListener(IVehicleDamageListener* listener) {
  listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener),
                   listeners_.end());
}

}  // namespace game
