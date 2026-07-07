#pragma once

#include "core/Color.h"

namespace vfx {

// Tunable parameters for a vfx::VFXExplosion instance.
//
// All fields carry sensible defaults so a caller can use VFXExplosionDesc{}
// as-is; only the fields that matter for a particular explosion (e.g. a
// bigger shockwave for a truck wreck) need to be overridden.
struct VFXExplosionDesc {
  // Total lifetime (s) of the effect, i.e. how long the fireball/smoke burst
  // is expected to take to fully fade out before VFXExplosion self-expires.
  // Ignored by the particle sub-systems themselves (they follow their own
  // authored duration/lifetime) — this only bounds IsFinished().
  float particle_duration = 2.5f;

  // Point light flash.
  core::Color light_color     = core::Color(1.f, 0.6f, 0.2f);
  float       light_intensity = 8.f;
  float       light_radius    = 15.f;
  float       light_lifetime  = 0.15f;

  // Physics shockwave: outward impulse (kg*m/s) applied to Dynamic bodies
  // within shockwave_radius metres, linearly falling off to 0 at the radius.
  float shockwave_radius  = 8.f;
  float shockwave_impulse = 2000.f;

  // Screen shake: see vfx::ScreenShake::SetBaseMagnitude/SetDuration.
  float shake_base_magnitude = 1.5f;
  float shake_duration       = 0.6f;
};

}  // namespace vfx
