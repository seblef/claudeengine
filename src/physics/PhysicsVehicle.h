#pragma once

#include <cstdint>

#include "core/Mat4f.h"
#include "core/Vec3f.h"

// Forward-declare Jolt internals so Jolt headers never leak into consumers.
namespace JPH {
class Body;
class PhysicsSystem;
class VehicleConstraint;
}  // namespace JPH

namespace physics {

class IPhysicsBodyListener;
class PhysicsSystem;

/// Contact information for a single wheel, queried after PhysicsSystem::Step().
struct WheelContact {
    // cppcheck-suppress unusedStructMember
    bool        has_contact;      ///< True when the wheel is touching the ground.
    // cppcheck-suppress unusedStructMember
    core::Vec3f position;         ///< World-space contact point (valid only when has_contact).
    // cppcheck-suppress unusedStructMember
    uint32_t    contact_body_id;  ///< Jolt BodyID packed as index+sequence (opaque to callers).
};

/// Opaque handle representing a simulated wheeled vehicle owned by PhysicsSystem.
///
/// Instances are created exclusively through PhysicsSystem::CreateVehicle() and
/// must not outlive the PhysicsSystem that owns them.
///
/// Typical per-frame usage:
///   1. Before PhysicsSystem::Step(): call Set*() to push driver input.
///   2. After  PhysicsSystem::Step(): call Get*WorldTransform() to read results.
class PhysicsVehicle {
 public:
    ~PhysicsVehicle();

    // ---- Driver input (call each frame before PhysicsSystem::Step()) ----------

    /// Throttle input in [-1, 1].  Negative values select reverse.
    void SetThrottle(float v);

    /// Steering input in [-1, 1].  Negative = left.
    void SetSteer(float v);

    /// Brake pedal pressure in [0, 1].
    void SetBrake(float v);

    /// Engage (true) or release (false) the hand brake.
    void SetHandbrake(bool on);

    // ---- Simulation output (call each frame after PhysicsSystem::Step()) -----

    /// Signed speed along the vehicle's forward axis (m/s).
    /// Positive = moving forward, negative = moving backward.
    [[nodiscard]] float GetForwardSpeed() const;

    /// World-space transform of the vehicle body origin.
    [[nodiscard]] core::Mat4f GetBodyWorldTransform() const;

    /// World-space transform of wheel at index:
    ///   0 = front-left, 1 = front-right, 2 = rear-left, 3 = rear-right.
    /// Returns a matrix that positions and orients a cylinder aligned with the
    /// local Y axis in model space (the standard Jolt wheel convention).
    [[nodiscard]] core::Mat4f GetWheelWorldTransform(int wheel_index) const;

    /// Returns contact information for wheel at index.
    /// Must be called after PhysicsSystem::Step().
    [[nodiscard]] WheelContact GetWheelContact(int wheel_index) const;

    /// Returns the number of wheels on this vehicle.
    [[nodiscard]] int GetWheelCount() const;

    /// Returns the physical width of wheel at index in metres (from WheelSettings::mWidth).
    [[nodiscard]] float GetWheelWidth(int wheel_index) const;

 private:
    friend class PhysicsSystem;

    /// Constructed only through PhysicsSystem::CreateVehicle().
    PhysicsVehicle(JPH::Body* body,
                   JPH::VehicleConstraint* constraint,
                   JPH::PhysicsSystem* jolt,
                   IPhysicsBodyListener* listener);

    // cppcheck-suppress unusedStructMember
    JPH::Body*              body_;         ///< Non-owning; lifetime managed by Jolt BodyInterface.
    // cppcheck-suppress unusedStructMember
    JPH::VehicleConstraint* constraint_;   ///< Ref-counted; AddRef in ctor, Release in dtor.
    // cppcheck-suppress unusedStructMember
    JPH::PhysicsSystem*     jolt_system_;  ///< Non-owning.
    // cppcheck-suppress unusedStructMember
    IPhysicsBodyListener*   listener_;     ///< Non-owning; may be nullptr.
    // cppcheck-suppress unusedStructMember
    float throttle_  = 0.f;
    // cppcheck-suppress unusedStructMember
    float steer_     = 0.f;
    // cppcheck-suppress unusedStructMember
    float brake_     = 0.f;
    // cppcheck-suppress unusedStructMember
    bool  handbrake_ = false;
};

}  // namespace physics
