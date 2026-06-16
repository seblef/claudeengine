#include "physics/PhysicsBody.h"

#include <cassert>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSystem.h>

namespace physics {

namespace {

// Extract the world-space position from an engine row-major transform.
// Translation resides in column 3.
JPH::RVec3 ExtractPosition(const core::Mat4f& m) {
    return JPH::RVec3(m(0, 3), m(1, 3), m(2, 3));
}

// Extract a rotation quaternion from the upper-left 3x3 of a row-major transform.
// JPH::Mat44 constructor takes four *column* vectors, so column c of M is
// {M(0,c), M(1,c), M(2,c)}.
JPH::Quat ExtractRotation(const core::Mat4f& m) {
    JPH::Mat44 rot(
        JPH::Vec4(m(0, 0), m(1, 0), m(2, 0), 0.0f),
        JPH::Vec4(m(0, 1), m(1, 1), m(2, 1), 0.0f),
        JPH::Vec4(m(0, 2), m(1, 2), m(2, 2), 0.0f),
        JPH::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    return rot.GetQuaternion();
}

}  // namespace

PhysicsBody::PhysicsBody(uint32_t body_id, MotionType motion_type,
                         IPhysicsBodyListener* listener,
                         JPH::PhysicsSystem* jolt)
    : body_id_(body_id),
      motion_type_(motion_type),
      listener_(listener),
      jolt_system_(jolt) {}

core::Mat4f PhysicsBody::GetWorldTransform() const {
    const JPH::RMat44 jm =
        jolt_system_->GetBodyInterface().GetWorldTransform(JPH::BodyID(body_id_));
    core::Mat4f result;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            result(row, col) = jm(static_cast<JPH::uint>(row),
                                  static_cast<JPH::uint>(col));
    return result;
}

void PhysicsBody::SetWorldTransform(const core::Mat4f& transform) {
    assert(motion_type_ != MotionType::Dynamic &&
           "SetWorldTransform must not be called on Dynamic bodies");

    const JPH::BodyID id(body_id_);
    const JPH::RVec3  pos = ExtractPosition(transform);
    const JPH::Quat   rot = ExtractRotation(transform);
    JPH::BodyInterface& iface = jolt_system_->GetBodyInterface();

    if (motion_type_ == MotionType::Static) {
        iface.SetPositionAndRotation(id, pos, rot, JPH::EActivation::DontActivate);
    } else {
        // MoveKinematic sets velocity = (target - current) / dt so that
        // kinematic bodies push dynamic ones correctly. We assume a nominal
        // 60 Hz physics step; a future overload can accept an explicit dt.
        constexpr float kNominalDt = 1.0f / 60.0f;
        iface.MoveKinematic(id, pos, rot, kNominalDt);
    }
}

void PhysicsBody::ApplyForce(const core::Vec3f& force) {
    assert(motion_type_ == MotionType::Dynamic &&
           "ApplyForce is only valid on Dynamic bodies");
    jolt_system_->GetBodyInterface().AddForce(
        JPH::BodyID(body_id_), JPH::Vec3(force.x, force.y, force.z));
}

void PhysicsBody::ApplyImpulse(const core::Vec3f& impulse) {
    assert(motion_type_ == MotionType::Dynamic &&
           "ApplyImpulse is only valid on Dynamic bodies");
    jolt_system_->GetBodyInterface().AddImpulse(
        JPH::BodyID(body_id_), JPH::Vec3(impulse.x, impulse.y, impulse.z));
}

void PhysicsBody::SetLinearVelocity(const core::Vec3f& velocity) {
    assert(motion_type_ == MotionType::Dynamic &&
           "SetLinearVelocity is only valid on Dynamic bodies");
    jolt_system_->GetBodyInterface().SetLinearVelocity(
        JPH::BodyID(body_id_), JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

}  // namespace physics
