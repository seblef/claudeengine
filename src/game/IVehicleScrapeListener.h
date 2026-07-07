#pragma once

#include "core/Vec3f.h"

namespace game {

// Observer notified of continuous panel/geometry contact, forwarded from
// GameVehicle::OnSustainedContact(). Kept vfx-free (game/ must not depend on
// vfx/, since vfx/ already depends on game/) so the concrete implementation
// (vfx::VehicleScrapeEffect, which drives a vfx::VFXScrape) can live above
// this module without creating a circular dependency.
//
// Non-owning: GameVehicle only holds a pointer set via SetScrapeListener();
// the caller that constructs the concrete listener also owns its lifetime,
// mirroring how IVehicleController is wired via SetVehicleController().
class IVehicleScrapeListener {
 public:
  virtual ~IVehicleScrapeListener() = default;

  // Advances any no-contact grace timer. Call once per frame.
  virtual void Update(float dt) = 0;

  // Called once per physics step while the panel is in contact with geometry.
  virtual void RegisterContact(const core::Vec3f& world_point,
                               const core::Vec3f& world_normal,
                               const core::Vec3f& relative_velocity) = 0;
};

}  // namespace game
