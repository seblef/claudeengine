#include "physics/PhysicsBody.h"

#include <cassert>
#include <cmath>

#include <Jolt/Jolt.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
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
// Normalizes each column to strip scale; GetQuaternion() requires unit columns.
JPH::Quat ExtractRotation(const core::Mat4f& m) {
    auto Col = [&](int c) {
        const float x = m(0, c), y = m(1, c), z = m(2, c);
        const float inv_len = 1.f / std::sqrt(x * x + y * y + z * z);
        return JPH::Vec4(x * inv_len, y * inv_len, z * inv_len, 0.f);
    };
    JPH::Mat44 rot(Col(0), Col(1), Col(2), JPH::Vec4(0.f, 0.f, 0.f, 1.f));
    return rot.GetQuaternion();
}

// Extract the per-axis scale from the upper-left 3x3 of a row-major transform
// (column lengths).
core::Vec3f ExtractScale(const core::Mat4f& m) {
    auto ColLen = [&](int c) {
        const float x = m(0, c), y = m(1, c), z = m(2, c);
        return std::sqrt(x * x + y * y + z * z);
    };
    return core::Vec3f(ColLen(0), ColLen(1), ColLen(2));
}

}  // namespace

PhysicsBody::PhysicsBody(uint32_t body_id, MotionType motion_type,
                         IPhysicsBodyListener* listener,
                         JPH::PhysicsSystem* jolt,
                         const JPH::Shape* base_shape,
                         const core::Vec3f& initial_scale)
    : body_id_(body_id),
      motion_type_(motion_type),
      listener_(listener),
      jolt_system_(jolt),
      base_shape_(base_shape),
      current_scale_(initial_scale) {
    if (base_shape_) base_shape_->AddRef();
}

PhysicsBody::~PhysicsBody() {
    if (base_shape_) base_shape_->Release();
}

core::Mat4f PhysicsBody::GetWorldTransform() const {
    const JPH::RMat44 jm =
        jolt_system_->GetBodyInterface().GetWorldTransform(JPH::BodyID(body_id_));
    core::Mat4f result;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            result(row, col) = jm(static_cast<JPH::uint>(row),
                                  static_cast<JPH::uint>(col));
    // Jolt stores only rotation+translation; re-inject the scale that was
    // baked into the shape at creation time (column lengths = scale factors).
    for (int row = 0; row < 3; ++row) {
        result(row, 0) *= current_scale_.x;
        result(row, 1) *= current_scale_.y;
        result(row, 2) *= current_scale_.z;
    }
    return result;
}

void PhysicsBody::SetWorldTransform(const core::Mat4f& transform) {
    assert(motion_type_ != MotionType::Dynamic &&
           "SetWorldTransform must not be called on Dynamic bodies");

    const JPH::BodyID   id    = JPH::BodyID(body_id_);
    const JPH::RVec3    pos   = ExtractPosition(transform);
    const JPH::Quat     rot   = ExtractRotation(transform);
    const core::Vec3f   scale = ExtractScale(transform);
    JPH::BodyInterface& iface = jolt_system_->GetBodyInterface();

    if (base_shape_ && scale != current_scale_) {
        current_scale_ = scale;
        const JPH::EActivation act =
            (motion_type_ == MotionType::Static) ? JPH::EActivation::DontActivate
                                                 : JPH::EActivation::Activate;
        constexpr float kUnity = 1.f;
        const bool is_identity = (std::abs(scale.x - kUnity) < 1e-5f &&
                                  std::abs(scale.y - kUnity) < 1e-5f &&
                                  std::abs(scale.z - kUnity) < 1e-5f);
        const JPH::ShapeRefC new_shape =
            is_identity
                ? JPH::ShapeRefC(base_shape_)
                : JPH::ShapeRefC(new JPH::ScaledShape(
                      base_shape_, JPH::Vec3(scale.x, scale.y, scale.z)));
        iface.SetShape(id, new_shape, false, act);
    }

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
