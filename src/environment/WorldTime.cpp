#include "environment/WorldTime.h"

#include <algorithm>
#include <cmath>

namespace environment {

namespace {

constexpr float kPi            = static_cast<float>(M_PI);
constexpr float kSecondsPerDay = 86400.f;
// Small fixed tilt offset applied to the moon direction (radians).
constexpr float kMoonTiltRad   = 0.1f;

// Returns hours in [0, 24) from a world_time in seconds.
inline float ToHours(float world_time) {
    return world_time / 3600.f;
}

// Computes the world-space sun direction from time of day, latitude and
// solar declination using the astronomical hour-angle formula.
//
// Engine coordinate system: +X east, +Y up, +Z south.
//
// Azimuth convention (standard, measured from north clockwise):
//   0   = north  → −Z
//   π/2 = east   → +X
//   π   = south  → +Z
//   3π/2= west   → −X
core::Vec3f ComputeSunDir(float hour, float latitude_deg, float declination_deg) {
    const float phi   = latitude_deg    * (kPi / 180.f);  // latitude (rad)
    const float delta = declination_deg * (kPi / 180.f);  // declination (rad)

    // Hour angle: 0 at solar noon, negative in the morning.
    const float H = (hour - 12.f) * (kPi / 12.f);

    // Solar elevation   sin(α) = sin(φ)sin(δ) + cos(φ)cos(δ)cos(H)
    const float sin_elev = std::sin(phi) * std::sin(delta)
                         + std::cos(phi) * std::cos(delta) * std::cos(H);
    const float elevation = std::asin(std::clamp(sin_elev, -1.f, 1.f));
    const float cos_el    = std::cos(elevation);

    // Solar azimuth from north (clockwise):
    //   cos(A) = (sin(δ) − sin(φ)·sin(α)) / (cos(φ)·cos(α))
    // For H < 0 (morning) A < π; for H > 0 (afternoon) A > π.
    float azimuth = 0.f;
    if (cos_el > 1e-4f) {
        const float cos_az = std::clamp(
            (std::sin(delta) - std::sin(phi) * sin_elev)
                / (std::cos(phi) * cos_el),
            -1.f, 1.f);
        azimuth = (H <= 0.f) ? std::acos(cos_az) : 2.f * kPi - std::acos(cos_az);
    }

    // Cartesian: A=0→north(−Z), A=π/2→east(+X), A=π→south(+Z), A=3π/2→west(−X)
    const float x =  std::sin(azimuth) * cos_el;
    const float y =  std::sin(elevation);
    const float z = -std::cos(azimuth) * cos_el;
    return core::Vec3f(x, y, z).Normalized();
}

}  // namespace

WorldTime::WorldTime(float time_scale, float start_time)
    : world_time_(start_time * 3600.f), time_scale_(time_scale) {}

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

void WorldTime::SetTimeOfDay(float hours) {
    world_time_ = hours * 3600.f;
    if (world_time_ < 0.f)             world_time_ = 0.f;
    if (world_time_ >= kSecondsPerDay) world_time_ = kSecondsPerDay - 1.f;
}

float WorldTime::GetTimeOfDay() const {
    return ToHours(world_time_);
}

core::Vec3f WorldTime::GetSunDirection() const {
    return ComputeSunDir(ToHours(world_time_), latitude_deg_, declination_deg_);
}

core::Vec3f WorldTime::GetMoonDirection() const {
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
