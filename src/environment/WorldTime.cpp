#include "environment/WorldTime.h"

#include <cmath>

namespace environment {

namespace {

constexpr float kSecondsPerDay   = 86400.f;
// Maximum solar elevation at noon for latitude 45° N on equinox (degrees).
constexpr float kMaxElevationDeg = 45.f;
// Solar elevation converted to radians.
constexpr float kMaxElevationRad = kMaxElevationDeg * (static_cast<float>(M_PI) / 180.f);
// Small fixed tilt offset applied to the moon direction (radians).
constexpr float kMoonTiltRad     = 0.1f;

// Returns hours in [0, 24) from a world_time in seconds.
inline float ToHours(float world_time) {
    return world_time / 3600.f;
}

}  // namespace

WorldTime::WorldTime(float time_scale) : time_scale_(time_scale) {}

void WorldTime::Advance(float real_dt) {
    world_time_ += real_dt * time_scale_;
    if (world_time_ >= kSecondsPerDay)
        world_time_ -= kSecondsPerDay;
    if (world_time_ < 0.f)
        world_time_ += kSecondsPerDay;
}

void WorldTime::SetTimeScale(float s) {
    time_scale_ = s;
}

float WorldTime::GetTimeOfDay() const {
    return ToHours(world_time_);
}

core::Vec3f WorldTime::GetSunDirection() const {
    // Hours mapped to angle: 6h → east (0 rad), 12h → south (π/2), 18h → west (π).
    // Azimuth angle increases from east toward west over the daytime arc.
    const float hour = ToHours(world_time_);

    // Solar azimuth: full 2π cycle over 24 h, east at 06:00.
    const float azimuth = ((hour - 6.f) / 24.f) * 2.f * static_cast<float>(M_PI);

    // Elevation: a sine wave peaking at noon.  Ranges from -kMaxElevationRad
    // at midnight to +kMaxElevationRad at noon.
    const float elevation = std::sin(((hour - 6.f) / 12.f) * static_cast<float>(M_PI))
                            * kMaxElevationRad;

    // Convert spherical to Cartesian (+X east, +Y up, +Z south).
    const float cos_el = std::cos(elevation);
    const float x = -std::cos(azimuth) * cos_el;   // east–west axis
    const float y =  std::sin(elevation);
    const float z = -std::sin(azimuth) * cos_el;   // north–south axis

    return core::Vec3f(x, y, z).Normalized();
}

core::Vec3f WorldTime::GetMoonDirection() const {
    // Moon is antipodal to the sun with a small tilt around the X axis.
    const core::Vec3f sun = GetSunDirection();
    const float cos_t = std::cos(kMoonTiltRad);
    const float sin_t = std::sin(kMoonTiltRad);
    return core::Vec3f(
        -sun.x,
        -sun.y * cos_t - (-sun.z) * sin_t,
        -sun.z * cos_t + (-sun.y) * sin_t
    ).Normalized();
}

bool WorldTime::IsDaytime() const {
    return GetSunDirection().y > 0.f;
}

}  // namespace environment
