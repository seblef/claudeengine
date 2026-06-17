#include "physics/CharacterController.h"

#include <loguru.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "physics/CollisionLayer.h"

namespace physics {

CharacterController::~CharacterController() = default;

namespace {

JPH::RVec3 ExtractPosition(const core::Mat4f& m) {
    return JPH::RVec3(m(0, 3), m(1, 3), m(2, 3));
}

JPH::Quat ExtractRotation(const core::Mat4f& m) {
    JPH::Mat44 rot(
        JPH::Vec4(m(0, 0), m(1, 0), m(2, 0), 0.0f),
        JPH::Vec4(m(0, 1), m(1, 1), m(2, 1), 0.0f),
        JPH::Vec4(m(0, 2), m(1, 2), m(2, 2), 0.0f),
        JPH::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    return rot.GetQuaternion();
}

core::Mat4f JoltMatToMat4f(const JPH::RMat44& m) {
    core::Mat4f result;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            result(row, col) = m(static_cast<JPH::uint>(row),
                                 static_cast<JPH::uint>(col));
    return result;
}

}  // namespace

CharacterController::CharacterController(float radius, float half_height,
                                         const core::Mat4f& initial_transform,
                                         JPH::PhysicsSystem* jolt,
                                         JPH::TempAllocator* allocator)
    : jolt_system_(jolt), temp_allocator_(allocator) {
    // Build a RotatedTranslatedShape so the capsule base sits at the character
    // origin (0, 0, 0).  The CapsuleShape center is mid-cylinder, so we shift
    // it up by half_height + radius (half cylinder + bottom hemisphere).
    JPH::Ref<JPH::CapsuleShape> capsule = new JPH::CapsuleShape(half_height, radius);
    JPH::RotatedTranslatedShapeSettings rts_settings(
        JPH::Vec3(0.0f, half_height + radius, 0.0f),
        JPH::Quat::sIdentity(),
        capsule);

    auto rts_result = rts_settings.Create();
    if (rts_result.HasError()) {
        LOG_F(FATAL, "CharacterController: shape creation failed: %s",
              rts_result.GetError().c_str());
        return;
    }

    JPH::CharacterVirtualSettings settings;
    settings.mShape          = rts_result.Get();
    settings.mUp             = JPH::Vec3::sAxisY();
    // Accept contacts with the lower hemisphere (base of the capsule).
    settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -radius);

    character_ = std::make_unique<JPH::CharacterVirtual>(
        &settings,
        ExtractPosition(initial_transform),
        ExtractRotation(initial_transform),
        jolt_system_);
}

void CharacterController::Move(const core::Vec3f& velocity, float dt) {
    character_->SetLinearVelocity(JPH::Vec3(velocity.x, velocity.y, velocity.z));
    character_->Update(
        dt,
        jolt_system_->GetGravity(),
        jolt_system_->GetDefaultBroadPhaseLayerFilter(kLayerPlayer),
        jolt_system_->GetDefaultLayerFilter(kLayerPlayer),
        {},
        {},
        *temp_allocator_);
}

core::Mat4f CharacterController::GetWorldTransform() const {
    return JoltMatToMat4f(character_->GetWorldTransform());
}

bool CharacterController::IsGrounded() const {
    return character_->IsSupported();
}

}  // namespace physics
