#pragma once

#include "game/DamageZone.h"

namespace game {

/// Observer notified whenever a VehicleDamage zone crosses one of its
/// configured damage thresholds. Gameplay systems (steering penalty, engine
/// slowdown, smoke, wreck state, mesh swaps) subscribe here instead of each
/// independently inspecting collisions.
class IVehicleDamageListener {
 public:
  virtual ~IVehicleDamageListener() = default;

  /// Called once per threshold crossed upward (pristine → destroyed) as a
  /// result of a single impact. May be called multiple times in a row for the
  /// same zone if a single impact crosses more than one threshold.
  /// @param zone      The zone that was hit.
  /// @param threshold The threshold fraction that was crossed, in (0, 1].
  /// @param fraction  Current damage fraction for the zone, in [0, 1]
  ///                  (0 = pristine, 1 = destroyed).
  virtual void OnDamageThresholdCrossed(DamageZone zone, float threshold,
                                        float fraction) = 0;
};

}  // namespace game
