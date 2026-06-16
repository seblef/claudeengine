#pragma once

#include "core/Vec2f.h"
#include "core/Vec3f.h"

namespace environment {

struct EnvironmentDesc;

// Time-varying wind simulation for the XZ plane.
//
// Produces a per-frame wind vector by adding a sinusoidal gust offset to a
// steady base wind:
//   gust = base_strength * gust_amplitude * sin(phase * 2π * gust_frequency)
//   wind = base_direction * (base_strength + gust)
//
// The wind vector is always in the XZ plane (Y = 0).
//
// Default gust parameters (applied when not overridden):
//   gust_amplitude  = 0.3  (30 % of base speed)
//   gust_frequency  = 0.2 Hz (period ≈ 5 s, slow gentle gusts)
//
// Usage:
//   WindSystem wind(desc);
//   // each frame:
//   wind.Update(dt);
//   core::Vec3f vec = wind.GetWindVector();
class WindSystem {
 public:
  explicit WindSystem(const EnvironmentDesc& desc);

  // Advances the wind phase by dt seconds and recomputes the wind vector.
  void Update(float dt);

  // Returns the current XZ wind vector (Y component is always 0).
  [[nodiscard]] core::Vec3f GetWindVector() const;

  // Returns the scalar magnitude of the current wind vector (m/s).
  [[nodiscard]] float GetWindStrength() const;

  // Returns accumulated simulation time in seconds (for shader phase effects).
  [[nodiscard]] float GetWindTime() const { return wind_time_; }

  // Returns accumulated XZ displacement in metres (time-integral of wind velocity).
  [[nodiscard]] core::Vec2f GetWindDisplacement() const { return wind_displacement_; }

  // Hot-updates the wind base direction (XZ unit vector; Y is ignored).
  void SetBaseDirection(const core::Vec3f& dir);

  // Hot-updates the base wind speed in m/s.
  void SetBaseStrength(float strength);

 private:
  // cppcheck-suppress unusedStructMember
  core::Vec3f base_direction_;              // unit XZ vector
  float       base_strength_    = 0.f;     // base wind speed in m/s
  float       gust_amplitude_   = 0.3f;    // gust amplitude as fraction of base_strength
  float       gust_frequency_   = 0.2f;    // gust frequency in Hz
  float       phase_            = 0.f;     // phase accumulator in seconds
  float       wind_time_        = 0.f;     // total elapsed time in seconds
  // cppcheck-suppress unusedStructMember
  core::Vec3f wind_vec_;                   // current wind vector (Y = 0)
  // cppcheck-suppress unusedStructMember
  core::Vec2f wind_displacement_;          // time-integral of wind_vec_.xz * dt (m)
};

}  // namespace environment
