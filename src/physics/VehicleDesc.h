#pragma once

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
    float       brake_torque      = 1500.f;              ///< Brake torque per wheel (Nm).
    // cppcheck-suppress unusedStructMember
    float       handbrake_torque  = 3000.f;              ///< Hand-brake torque (rear wheels only, Nm).
    // cppcheck-suppress unusedStructMember
    WheelDesc   front_left;
    // cppcheck-suppress unusedStructMember
    WheelDesc   front_right;
    // cppcheck-suppress unusedStructMember
    WheelDesc   rear_left;
    // cppcheck-suppress unusedStructMember
    WheelDesc   rear_right;
};

}  // namespace physics
