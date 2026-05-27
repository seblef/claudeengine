#pragma once

#include "core/Vec3f.h"

namespace environment {

// Tracks in-game time of day (0–86400 s) with a configurable time scale.
//
// Celestial body directions assume latitude 45° N. The sun arcs from east
// (06:00) through zenith (12:00, elevation ≈ 60°) to west (18:00), and is
// below the horizon from 18:00 to 06:00. The moon is approximated as the
// antipodal sun with a small fixed tilt offset.
class WorldTime {
 public:
    explicit WorldTime(float time_scale = 1.f);

    // Advances world time by real_dt seconds multiplied by the time scale.
    // Wraps at 86400 s (one full day).
    void Advance(float real_dt);

    void SetTimeScale(float s);

    // Returns the time of day in hours [0, 24).
    [[nodiscard]] float GetTimeOfDay() const;

    // Returns the world-space unit vector pointing toward the sun.
    [[nodiscard]] core::Vec3f GetSunDirection() const;

    // Returns the world-space unit vector pointing toward the moon.
    // Approximated as the antipodal sun with a small fixed tilt.
    [[nodiscard]] core::Vec3f GetMoonDirection() const;

    // Returns true when the sun is above the horizon.
    [[nodiscard]] bool IsDaytime() const;

 private:
    float world_time_ = 0.f;   // seconds, [0, 86400)
    float time_scale_ = 1.f;
};

}  // namespace environment
