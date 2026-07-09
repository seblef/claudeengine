#pragma once

#include "core/Vec3f.h"

namespace game {

// Observer notified exactly once when a vehicle transitions into the wrecked
// state — i.e. when GameVehicle::OnDamageThresholdCrossed() sees the wreck
// threshold (conventionally 1.0, the last entry of
// physics::VehicleDamageDesc::thresholds) crossed for the first time. Lets a
// vfx:: implementation (typically vfx::VehicleWreckEffect) trigger the
// explosion + wreck SFX payoff without game/ depending on vfx/ — see
// IVehicleFireListener.h for the rationale behind this split.
//
// Non-owning: GameVehicle only holds a pointer set via SetWreckListener();
// the caller that constructs the concrete listener also owns its lifetime,
// mirroring how IVehicleFireListener / IVehicleScrapeListener are wired.
class IVehicleWreckListener {
 public:
  virtual ~IVehicleWreckListener() = default;

  // Called once, the instant the vehicle is wrecked. world_position is the
  // vehicle body's world-space position, suitable for spawning an explosion.
  virtual void OnVehicleWrecked(const core::Vec3f& world_position) = 0;
};

}  // namespace game
