#pragma once

#include <cstdint>

#include "core/Mat4f.h"
#include "core/Vec3f.h"
#include "physics/IPhysicsBodyListener.h"
#include "physics/MotionType.h"

// Forward-declare Jolt internals so Jolt headers never leak into consumers.
namespace JPH {
class PhysicsSystem;
class Shape;
}  // namespace JPH

namespace physics {

class PhysicsSystem;

/// Opaque handle representing a simulated body owned by PhysicsSystem.
/// Instances are created exclusively through PhysicsSystem::CreateBody() and
/// friends, and must not outlive the PhysicsSystem that owns them.
class PhysicsBody {
 public:
    ~PhysicsBody();

    /// Read the current world-space transform from the simulation.
    /// Valid for all motion types.
    [[nodiscard]] core::Mat4f GetWorldTransform() const;

    /// Push a new world-space transform into the simulation.
    ///   Static:    teleports the body (SetPositionAndRotation, no activation).
    ///   Kinematic: drives the body toward the target over one physics step
    ///              (MoveKinematic at a nominal 60 Hz dt).
    /// Scale is extracted from the matrix and applied by replacing the body's
    /// shape with a ScaledShape wrapper when the scale changes.
    /// Must not be called on Dynamic bodies (asserts in debug builds).
    void SetWorldTransform(const core::Mat4f& transform);

    /// Apply a continuous force (N) at the centre of mass.
    /// Dynamic bodies only — asserts in debug builds otherwise.
    void ApplyForce(const core::Vec3f& force);

    /// Apply an impulse (kg·m/s) at the centre of mass.
    /// Dynamic bodies only — asserts in debug builds otherwise.
    void ApplyImpulse(const core::Vec3f& impulse);

    /// Set the linear velocity (m/s) directly.
    /// Dynamic bodies only — asserts in debug builds otherwise.
    void SetLinearVelocity(const core::Vec3f& velocity);

    [[nodiscard]] MotionType            GetMotionType() const { return motion_type_; }
    [[nodiscard]] IPhysicsBodyListener* GetListener()   const { return listener_; }

 private:
    friend class PhysicsSystem;

    /// Created only through PhysicsSystem::CreateBody() and friends.
    PhysicsBody(uint32_t body_id, MotionType motion_type,
                IPhysicsBodyListener* listener, JPH::PhysicsSystem* jolt,
                const JPH::Shape* base_shape, const core::Vec3f& initial_scale);

    // cppcheck-suppress unusedStructMember
    uint32_t              body_id_;       // stores JPH::BodyID value
    // cppcheck-suppress unusedStructMember
    MotionType            motion_type_;
    // cppcheck-suppress unusedStructMember
    IPhysicsBodyListener* listener_;      // non-owning
    // cppcheck-suppress unusedStructMember
    JPH::PhysicsSystem*   jolt_system_;   // non-owning
    // cppcheck-suppress unusedStructMember
    const JPH::Shape*     base_shape_;    // unscaled base shape; ref held via AddRef/Release
    // cppcheck-suppress unusedStructMember
    core::Vec3f           current_scale_;  // scale currently applied to the Jolt body
};

}  // namespace physics
