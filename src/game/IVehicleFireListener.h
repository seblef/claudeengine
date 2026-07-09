#pragma once

#include "core/Mat4f.h"

namespace game {

// Observer notified once per frame with the vehicle's current world
// transform, so an effect attached to the body (typically vfx::VFXFire, via
// vfx::VehicleFireEffect) can track it continuously. Unlike
// IVehicleScrapeListener, which is driven by discrete per-physics-step
// contact events, a fire/smoke effect has no natural "event" to hook into —
// it just needs the body's transform every frame for as long as it stays
// attached.
//
// Kept vfx-free (game/ must not depend on vfx/, since vfx/ already depends
// on game/) so the concrete implementation (vfx::VehicleFireEffect, which
// drives a vfx::VFXFire) can live above this module without creating a
// circular dependency.
//
// Non-owning: GameVehicle only holds a pointer set via SetFireListener();
// the caller that constructs the concrete listener also owns its lifetime,
// mirroring how IVehicleScrapeListener is wired via SetScrapeListener().
class IVehicleFireListener {
 public:
  virtual ~IVehicleFireListener() = default;

  // Called once per frame regardless of whether a fire effect is currently
  // active — the listener itself decides what to do with dt and the
  // transform. dt is needed so the listener can track how long its effect
  // has been burning (e.g. to enforce a minimum visible duration).
  virtual void OnVehicleTransformUpdated(float dt, const core::Mat4f& world_transform) = 0;
};

}  // namespace game
