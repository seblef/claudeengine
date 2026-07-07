#pragma once

namespace game {

// Discriminator for a vehicle body-local damage zone. Order matches
// physics::VehicleDamageDesc::zone_max_hp and physics::VehicleDamageDesc::
// thresholds — kept in sync so both can be indexed by static_cast<int>.
enum class DamageZone {
  kFront,
  kRear,
  kLeft,
  kRight,
  kRoof,
};

}  // namespace game
