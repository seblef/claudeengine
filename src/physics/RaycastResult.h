#pragma once

#include "core/Vec3f.h"

namespace physics {

// Forward declaration — full type defined in physics/PhysicsBody.h (downstream issue).
class PhysicsBody;

/// Result returned by a successful raycast query.
struct RaycastResult {
    // cppcheck-suppress unusedStructMember
    core::Vec3f  position;  ///< World-space hit position.
    // cppcheck-suppress unusedStructMember
    core::Vec3f  normal;    ///< World-space surface normal at the hit point.
    // cppcheck-suppress unusedStructMember
    float        distance;  ///< Ray-parametric distance from origin to hit point.
    // cppcheck-suppress unusedStructMember
    PhysicsBody* body;      ///< Body that was hit; null if no intersection.
};

}  // namespace physics
