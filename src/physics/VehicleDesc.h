#pragma once

#include <array>
#include <string>

#include "core/Vec3f.h"

namespace physics {

/// Wheel dimensions inferred from the wheel mesh bounding box.
/// Wheel meshes must be authored with their axle along X:
///   BBox X extent = width, BBox Y/Z extent = 2 * radius.
struct WheelGeometry {
    // cppcheck-suppress unusedStructMember
    float radius = 0.35f;  ///< Wheel radius (m), inferred from mesh bbox Y half-extent.
    // cppcheck-suppress unusedStructMember
    float width  = 0.2f;   ///< Wheel width (m), inferred from mesh bbox X extent.
};

/// Describes a single wheel's suspension properties and attachment point.
/// Fully Jolt-free; may be serialised to YAML.
/// Geometry (radius, width) is intentionally absent — use WheelGeometry inferred
/// from the wheel mesh bounding box instead.
struct WheelDesc {
    // cppcheck-suppress unusedStructMember
    core::Vec3f position;                        ///< Attachment point in vehicle body-local space.
    // cppcheck-suppress unusedStructMember
    float       suspension_min_length = 0.1f;   ///< Minimum suspension length at full compression (m).
    // cppcheck-suppress unusedStructMember
    float       suspension_max_length = 0.5f;   ///< Maximum suspension length at full droop (m).
    // cppcheck-suppress unusedStructMember
    float       suspension_stiffness  = 1500.f;  ///< Spring stiffness k (N/m).
    // cppcheck-suppress unusedStructMember
    float       suspension_damping    = 200.f;  ///< Spring damping c (N·s/m).
    // cppcheck-suppress unusedStructMember
    bool        is_driven            = false;   ///< True for engine-powered wheels.
    // cppcheck-suppress unusedStructMember
    bool        is_steered           = false;   ///< True for steering wheels.
};

/// Configures the per-zone progressive damage model (see game::VehicleDamage).
/// Zone order matches game::DamageZone: Front, Rear, Left, Right, Roof.
/// Fully Jolt-free; may be serialised to YAML.
struct VehicleDamageDesc {
    // cppcheck-suppress unusedStructMember
    std::array<float, 5> zone_max_hp = {100.f, 100.f, 100.f, 100.f, 100.f};
    // cppcheck-suppress unusedStructMember
    float impulse_to_damage_scale = 0.05f;  ///< Damage (HP) per unit of collision impulse (kg·m/s).
    // Fractions of zone HP (ascending) at which listeners are notified of a
    // damage-threshold crossing, e.g. {0.4, 0.6, 0.8, 1.0}.
    // cppcheck-suppress unusedStructMember
    std::array<float, 4> thresholds = {0.4f, 0.6f, 0.8f, 1.0f};
};

/// Configures the impulse-driven crash sound bank (see game::VehicleCrashSound).
/// Fully Jolt-free; may be serialised to YAML.
struct CrashSoundDesc {
    // cppcheck-suppress unusedStructMember
    float min_impulse    = 150.f;   ///< Impulse (kg·m/s) below which no crash sound plays.
    // cppcheck-suppress unusedStructMember
    float medium_impulse = 500.f;   ///< Impulse at/above which the medium sample plays instead of light.
    // cppcheck-suppress unusedStructMember
    float heavy_impulse  = 1200.f;  ///< Impulse at/above which the heavy sample plays.
    // cppcheck-suppress unusedStructMember
    float max_impulse    = 3000.f;  ///< Impulse at which gain saturates to 1.0.
    // cppcheck-suppress unusedStructMember
    float debounce_time  = 0.2f;    ///< Minimum time (s) between two crash sound triggers.
    // cppcheck-suppress unusedStructMember
    std::string light_sound  = "crash_light";   ///< Sound asset stem for the light tier.
    // cppcheck-suppress unusedStructMember
    std::string medium_sound = "crash_medium";  ///< Sound asset stem for the medium tier.
    // cppcheck-suppress unusedStructMember
    std::string heavy_sound  = "crash_heavy";   ///< Sound asset stem for the heavy tier.
};

/// Configures the continuous scrape effect (see vfx::VFXScrape,
/// vfx::VehicleScrapeEffect): directional sparks + a looping metal screech
/// while a panel slides along geometry at speed.
/// Fully Jolt-free; may be serialised to YAML.
struct ScrapeDesc {
    // cppcheck-suppress unusedStructMember
    float min_speed = 0.5f;   ///< Sliding speed (m/s) below which the scrape stops.
    // cppcheck-suppress unusedStructMember
    float max_speed = 15.f;   ///< Speed at which spark rate / screech gain saturate to 1.0.
    // cppcheck-suppress unusedStructMember
    float base_emission_rate = 60.f;  ///< Spark particles/sec at max_speed.
    // cppcheck-suppress unusedStructMember
    float base_gain = 1.f;    ///< Screech gain multiplier at max_speed.
    // cppcheck-suppress unusedStructMember
    float contact_grace_time = 0.15f;  ///< Time (s) a contact gap is tolerated before stopping.
    // cppcheck-suppress unusedStructMember
    std::string screech_sound = "scratching_metal";  ///< Sound asset stem for the loop.
};

/// Top-level description of a wheeled vehicle.
/// Fully Jolt-free; may be serialised to YAML.
struct VehicleDesc {
    // cppcheck-suppress unusedStructMember
    float       mass              = 1200.f;              ///< Vehicle mass (kg).
    // cppcheck-suppress unusedStructMember
    core::Vec3f half_extents      = {1.f, 0.5f, 2.f};   ///< Body box half-extents (m).
    // cppcheck-suppress unusedStructMember
    core::Vec3f com_offset        = {0.f, -0.3f, 0.f};  ///< Centre-of-mass offset from body origin (m).
    // cppcheck-suppress unusedStructMember
    float       max_engine_torque = 300.f;               ///< Peak engine torque (Nm).
    // cppcheck-suppress unusedStructMember
    float       max_steer_angle   = 0.5f;                ///< Maximum steering angle for steered wheels (rad).
    // cppcheck-suppress unusedStructMember
    float       min_steer_scale   = 0.4f;                ///< Steer input multiplier at/above high_speed_reference_speed
    // cppcheck-suppress unusedStructMember
    float       high_speed_reference_speed = 25.f;       ///< Speed (m/s) at which steer scale reaches min_steer_scale.
    // cppcheck-suppress unusedStructMember
    float       brake_torque      = 1500.f;              ///< Brake torque per wheel (Nm).
    // cppcheck-suppress unusedStructMember
    float       handbrake_torque  = 3000.f;              ///< Hand-brake torque (rear wheels only, Nm).
    // cppcheck-suppress unusedStructMember
    float       engine_inertia    = 0.1f;    ///< Engine rotational inertia (kg·m²). Lower = faster rev response.
    // cppcheck-suppress unusedStructMember
    float       gear_switch_time  = 0.1f;    ///< Time to complete a gear change (s). 0 = instant.
    // cppcheck-suppress unusedStructMember
    float       clutch_strength   = 40.0f;   ///< Clutch engagement strength. Higher = snappier power delivery.
    // cppcheck-suppress unusedStructMember
    WheelDesc   front_left;
    // cppcheck-suppress unusedStructMember
    WheelDesc   front_right;
    // cppcheck-suppress unusedStructMember
    WheelDesc   rear_left;
    // cppcheck-suppress unusedStructMember
    WheelDesc   rear_right;
    // cppcheck-suppress unusedStructMember
    VehicleDamageDesc damage;
    // cppcheck-suppress unusedStructMember
    CrashSoundDesc crash_sound;
    // cppcheck-suppress unusedStructMember
    ScrapeDesc scrape;
};

}  // namespace physics
