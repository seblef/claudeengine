#include "physics/PhysicsVehicle.h"

#include <Jolt/Jolt.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/Wheel.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include "physics/IPhysicsBodyListener.h"

namespace physics {

namespace {

core::Mat4f JoltMatToMat4f(const JPH::RMat44& m) {
    core::Mat4f result;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            result(row, col) = m(static_cast<JPH::uint>(row),
                                 static_cast<JPH::uint>(col));
    return result;
}

}  // namespace

PhysicsVehicle::PhysicsVehicle(JPH::Body* body,
                                JPH::VehicleConstraint* constraint,
                                JPH::PhysicsSystem* jolt,
                                IPhysicsBodyListener* listener)
    : body_(body),
      constraint_(constraint),
      jolt_system_(jolt),
      listener_(listener) {
    constraint_->AddRef();

    // Push driver input into the controller at the start of each physics sub-step.
    constraint_->SetPreStepCallback(
        [this](JPH::VehicleConstraint& vc,
               const JPH::PhysicsStepListenerContext&) {
            auto* ctrl =
                static_cast<JPH::WheeledVehicleController*>(vc.GetController());
            ctrl->SetDriverInput(throttle_, steer_, brake_,
                                 handbrake_ ? 1.f : 0.f);
        });
}

PhysicsVehicle::~PhysicsVehicle() {
    const JPH::BodyID body_id = body_->GetID();

    // Remove the constraint before the body so the constraint destructor can
    // still access a valid body pointer.
    jolt_system_->RemoveStepListener(constraint_);
    jolt_system_->RemoveConstraint(constraint_);
    constraint_->Release();

    JPH::BodyInterface& iface = jolt_system_->GetBodyInterface();
    iface.RemoveBody(body_id);
    iface.DestroyBody(body_id);
}

void PhysicsVehicle::SetThrottle(float v)  { throttle_  = v; }
void PhysicsVehicle::SetSteer(float v)     { steer_     = v; }
void PhysicsVehicle::SetBrake(float v)     { brake_     = v; }
void PhysicsVehicle::SetHandbrake(bool on) { handbrake_ = on; }

float PhysicsVehicle::GetForwardSpeed() const {
    const JPH::Vec3 vel     = body_->GetLinearVelocity();
    const JPH::Vec3 forward = body_->GetRotation() * JPH::Vec3(0.f, 0.f, 1.f);
    return vel.Dot(forward);
}

core::Mat4f PhysicsVehicle::GetBodyWorldTransform() const {
    return JoltMatToMat4f(body_->GetWorldTransform());
}

core::Mat4f PhysicsVehicle::GetWheelWorldTransform(int wheel_index) const {
    // Jolt's default wheel basis has local_right = forward × up = +Z × +Y = -X.
    // Passing +X as inWheelRight causes rotational_to_local to apply an implicit
    // 180° Y-rotation to every wheel.  Pass -X so the two -X factors cancel and
    // the returned transform is a pure translation + wheel-spin, letting
    // GameVehicle::Update apply MirrorY only for right-side wheels as intended.
    return JoltMatToMat4f(constraint_->GetWheelWorldTransform(
        static_cast<JPH::uint>(wheel_index),
        -JPH::Vec3::sAxisX(),
        JPH::Vec3::sAxisY()));
}

WheelContact PhysicsVehicle::GetWheelContact(int wheel_index) const {
    const JPH::Wheel* w = constraint_->GetWheel(static_cast<JPH::uint>(wheel_index));
    WheelContact result;
    result.has_contact     = w->HasContact();
    result.contact_body_id = w->GetContactBodyID().GetIndexAndSequenceNumber();
    if (result.has_contact) {
        const JPH::RVec3 pos = w->GetContactPosition();
        result.position = core::Vec3f(
            static_cast<float>(pos.GetX()),
            static_cast<float>(pos.GetY()),
            static_cast<float>(pos.GetZ()));
    }
    return result;
}

int PhysicsVehicle::GetWheelCount() const {
    return static_cast<int>(constraint_->GetWheels().size());
}

float PhysicsVehicle::GetWheelWidth(int wheel_index) const {
    return constraint_->GetWheel(static_cast<JPH::uint>(wheel_index))
               ->GetSettings()->mWidth;
}

void PhysicsVehicle::SetBodyTransform(const core::Mat4f& transform) {
    // Extract position from column 3.
    const JPH::RVec3 pos(
        static_cast<JPH::Real>(transform(0, 3)),
        static_cast<JPH::Real>(transform(1, 3)),
        static_cast<JPH::Real>(transform(2, 3)));
    // Build a Jolt rotation matrix from the upper-left 3×3.
    // Jolt Mat44 constructor takes column vectors; our column j = (row0, row1, row2).
    const JPH::Mat44 rot(
        JPH::Vec4(transform(0, 0), transform(1, 0), transform(2, 0), 0.f),
        JPH::Vec4(transform(0, 1), transform(1, 1), transform(2, 1), 0.f),
        JPH::Vec4(transform(0, 2), transform(1, 2), transform(2, 2), 0.f),
        JPH::Vec4(0.f, 0.f, 0.f, 1.f));
    jolt_system_->GetBodyInterface().SetPositionAndRotation(
        body_->GetID(), pos, rot.GetQuaternion(), JPH::EActivation::Activate);
}

void PhysicsVehicle::ZeroVelocities() {
    JPH::BodyInterface& iface = jolt_system_->GetBodyInterface();
    iface.SetLinearVelocity(body_->GetID(), JPH::Vec3::sZero());
    iface.SetAngularVelocity(body_->GetID(), JPH::Vec3::sZero());
}

}  // namespace physics
