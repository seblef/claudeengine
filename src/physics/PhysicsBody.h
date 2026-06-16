#pragma once

#include <cstdint>

#include "physics/IPhysicsBodyListener.h"
#include "physics/MotionType.h"

namespace physics {

class PhysicsSystem;

/// Opaque handle representing a simulated body registered in the physics world.
/// Instances are created by PhysicsSystem::CreateBody() (downstream issue) and
/// must not outlive the PhysicsSystem that owns them.
class PhysicsBody {
 public:
    PhysicsBody(MotionType motion_type, IPhysicsBodyListener* listener)
        : motion_type_(motion_type), listener_(listener) {}

    [[nodiscard]] MotionType GetMotionType() const { return motion_type_; }

    /// Listener that receives transform updates after each simulation step.
    /// May be nullptr if transform dispatch is not needed.
    [[nodiscard]] IPhysicsBodyListener* GetListener() const { return listener_; }

 private:
    friend class PhysicsSystem;

    // cppcheck-suppress unusedStructMember
    uint32_t              jolt_id_    = 0xffffffff;  // JPH::BodyID::cInvalidBodyID
    // cppcheck-suppress unusedStructMember
    MotionType            motion_type_;
    // cppcheck-suppress unusedStructMember
    IPhysicsBodyListener* listener_;
};

}  // namespace physics
