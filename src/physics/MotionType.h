#pragma once

namespace physics {

/// Describes how a physics body is simulated.
enum class MotionType {
    Static,     ///< Immovable; never moved by the physics solver.
    Kinematic,  ///< Moved by user code; solver-reads but does not integrate.
    Dynamic,    ///< Fully simulated; affected by forces and constraints.
};

}  // namespace physics
