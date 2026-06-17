#pragma once

#include <memory>

#include "core/Mat4f.h"
#include "core/Vec3f.h"

// Forward-declare Jolt internals so Jolt headers never leak into consumers.
namespace JPH {
class CharacterVirtual;
class PhysicsSystem;
class TempAllocator;
}  // namespace JPH

namespace physics {

class PhysicsSystem;

/// Kinematic character capsule backed by Jolt CharacterVirtual.
///
/// Created exclusively through PhysicsSystem::CreateCharacter().
/// The caller owns the returned unique_ptr; destroying it removes the character
/// from the simulation.  Must not outlive the PhysicsSystem that created it.
class CharacterController {
 public:
    ~CharacterController();

    /// Move the character by the given world-space velocity over dt seconds.
    /// Game code is responsible for incorporating gravity into inVelocity when
    /// the character is airborne.
    void Move(const core::Vec3f& velocity, float dt);

    /// World-space transform (position + orientation) of the capsule base centre.
    [[nodiscard]] core::Mat4f GetWorldTransform() const;

    /// True when the character is standing on a surface (flat or steep slope).
    [[nodiscard]] bool IsGrounded() const;

 private:
    friend class PhysicsSystem;

    CharacterController(float radius, float half_height,
                        const core::Mat4f& initial_transform,
                        JPH::PhysicsSystem* jolt,
                        JPH::TempAllocator* allocator);

    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::CharacterVirtual> character_;
    // cppcheck-suppress unusedStructMember
    JPH::PhysicsSystem* jolt_system_;    // non-owning
    // cppcheck-suppress unusedStructMember
    JPH::TempAllocator* temp_allocator_;  // non-owning
};

}  // namespace physics
