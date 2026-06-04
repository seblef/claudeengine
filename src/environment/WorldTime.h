#pragma once

#include "core/Vec3f.h"

namespace environment {

// Tracks in-game time of day (0–86400 s) with a configurable time scale.
//
// Sun and moon directions are computed from the geographic position using the
// standard astronomical hour-angle formula:
//   sin(α) = sin(φ)·sin(δ) + cos(φ)·cos(δ)·cos(H)
// where α = elevation, φ = latitude, δ = solar declination, H = hour angle.
//
// Defaults: latitude 45° N, equinox (declination 0°).  Adjust via
// SetLatitude() / SetDeclination() to tune the noon sun height and sky colour.
// Tip: summer solstice (declination +23.45°) raises the noon sun significantly
// and produces a noticeably bluer midday sky.
//
// The moon is approximated as the antipodal sun with a small fixed tilt.
class WorldTime {
 public:
    // time_scale: simulation multiplier (1.0 = real time).
    // start_time: starting time of day in hours [0, 24); defaults to noon.
    explicit WorldTime(float time_scale = 1.f, float start_time = 12.f);

    // Advances world time by real_dt seconds multiplied by the time scale.
    // Wraps at 86400 s (one full day).
    void Advance(float real_dt);

    void SetTimeScale(float s);

    // Sets the time of day directly (hours in [0, 24)).
    void SetTimeOfDay(float hours);

    // Returns the time of day in hours [0, 24).
    [[nodiscard]] float GetTimeOfDay() const;

    // Geographic latitude in degrees.  Positive = north hemisphere.
    // Lower latitudes (closer to 0°) raise the noon sun and produce a bluer sky.
    void SetLatitude(float degrees)    { latitude_deg_    = degrees; }

    // Solar declination in degrees.  0° = equinox, +23.45° = northern summer
    // solstice, −23.45° = southern summer solstice.  Higher declination raises
    // the noon sun for northern-hemisphere latitudes.
    void SetDeclination(float degrees) { declination_deg_ = degrees; }

    [[nodiscard]] float GetLatitude()    const { return latitude_deg_; }
    [[nodiscard]] float GetDeclination() const { return declination_deg_; }

    // Returns the world-space unit vector pointing toward the sun.
    // Engine coordinates: +X east, +Y up, +Z south.
    [[nodiscard]] core::Vec3f GetSunDirection() const;

    // Returns the world-space unit vector pointing toward the moon.
    // Approximated as the antipodal sun with a small fixed tilt.
    [[nodiscard]] core::Vec3f GetMoonDirection() const;

    // Returns true when the sun is above the horizon.
    [[nodiscard]] bool IsDaytime() const;

 private:
    // cppcheck-suppress unusedStructMember
    float world_time_;                    // seconds, [0, 86400)
    float time_scale_     = 1.f;
    float latitude_deg_   = 45.f;        // geographic latitude
    float declination_deg_ = 0.f;        // solar declination (season)
};

}  // namespace environment
