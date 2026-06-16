#pragma once

namespace physics {

/// Physical surface properties assigned to a body.
struct PhysicsMaterialDesc {
    float friction        = 0.5f;   ///< Coulomb friction coefficient [0, 1].
    float restitution     = 0.0f;   ///< Coefficient of restitution (bounciness) [0, 1].
    float mass            = 1.0f;   ///< Body mass in kilograms (Dynamic bodies only).
    float linear_damping  = 0.05f;  ///< Linear velocity damping per second.
    float angular_damping = 0.05f;  ///< Angular velocity damping per second.
    float gravity_factor  = 1.0f;   ///< Multiplier applied to world gravity for this body.
};

}  // namespace physics
