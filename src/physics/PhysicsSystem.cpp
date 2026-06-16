#include "physics/PhysicsSystem.h"

#include <algorithm>
#include <thread>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "core/Mat4f.h"
#include "physics/CollisionLayer.h"
#include "physics/MotionType.h"

namespace physics {

namespace {

// Two broad-phase buckets: all static layers map to 0, all dynamic layers to 1.
constexpr JPH::BroadPhaseLayer kBPLayerStatic(0);
constexpr JPH::BroadPhaseLayer kBPLayerDynamic(1);
constexpr JPH::uint            kBPLayerCount = 2;

/// Holds the three filter objects that must outlive the JPH::PhysicsSystem.
/// Allocated on the heap via unique_ptr because Jolt filters are NonCopyable.
struct LayerFilters {
    std::unique_ptr<JPH::BroadPhaseLayerInterfaceTable>      bp_iface;
    std::unique_ptr<JPH::ObjectLayerPairFilterTable>         obj_filter;
    std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilterTable> obj_vs_bp;

    LayerFilters() {
        // cppcheck-suppress useInitializationList
        // Broad-phase layer mapping: World → static bucket; others → dynamic.
        bp_iface = std::make_unique<JPH::BroadPhaseLayerInterfaceTable>(
            kLayerCount, kBPLayerCount);
        bp_iface->MapObjectToBroadPhaseLayer(kLayerWorld,      kBPLayerStatic);
        bp_iface->MapObjectToBroadPhaseLayer(kLayerPlayer,     kBPLayerDynamic);
        bp_iface->MapObjectToBroadPhaseLayer(kLayerEnemy,      kBPLayerDynamic);
        bp_iface->MapObjectToBroadPhaseLayer(kLayerProjectile, kBPLayerDynamic);

        // Object-layer pair filter: static–static never collide; all else do.
        obj_filter =
            std::make_unique<JPH::ObjectLayerPairFilterTable>(kLayerCount);
        for (uint16_t a = 0; a < kLayerCount; ++a) {
            for (uint16_t b = a; b < kLayerCount; ++b) {
                if (a == kLayerWorld && b == kLayerWorld) continue;
                obj_filter->EnableCollision(a, b);
            }
        }

        // Object-vs-BroadPhase filter derived from the two tables above.
        obj_vs_bp = std::make_unique<JPH::ObjectVsBroadPhaseLayerFilterTable>(
            *bp_iface, kBPLayerCount, *obj_filter, kLayerCount);
    }
};

/// Converts a Jolt row-column-indexed matrix to the engine's row-major Mat4f.
/// JPH::Mat44::operator()(row, col) maps to the same logical element as
/// core::Mat4f::operator()(row, col), so a direct copy is correct.
core::Mat4f JoltMatToMat4f(const JPH::RMat44& m) {
    core::Mat4f result;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result(row, col) = m(static_cast<JPH::uint>(row),
                                 static_cast<JPH::uint>(col));
        }
    }
    return result;
}

}  // namespace

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() {
    // Cleanup in reverse-init order.
    jolt_system_.reset();
    job_system_.reset();
    temp_allocator_.reset();
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void PhysicsSystem::Init() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    constexpr int kTempAllocatorSizeBytes = 8 * 1024 * 1024;  // 8 MB
    temp_allocator_ =
        std::make_unique<JPH::TempAllocatorImpl>(kTempAllocatorSizeBytes);

    const int num_threads =
        std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    job_system_ = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, num_threads);

    static LayerFilters filters;  // outlives jolt_system_

    constexpr JPH::uint kMaxBodies             = 65536;
    constexpr JPH::uint kMaxBodyPairs          = 65536;
    constexpr JPH::uint kMaxContactConstraints = 10240;
    constexpr JPH::uint kBodyMutexCount        = 0;  // auto

    jolt_system_ = std::make_unique<JPH::PhysicsSystem>();
    jolt_system_->Init(kMaxBodies, kBodyMutexCount, kMaxBodyPairs,
                       kMaxContactConstraints, *filters.bp_iface,
                       *filters.obj_vs_bp, *filters.obj_filter);
    jolt_system_->SetGravity(JPH::Vec3(0.f, -9.81f, 0.f));
}

void PhysicsSystem::Step(float dt) {
    jolt_system_->Update(dt, 1, temp_allocator_.get(), job_system_.get());

    JPH::BodyInterface& body_iface = jolt_system_->GetBodyInterface();
    for (PhysicsBody* body : bodies_) {
        if (body->motion_type_ != MotionType::Dynamic) continue;
        const JPH::BodyID id(body->jolt_id_);
        if (id.IsInvalid()) continue;
        const JPH::RMat44 jolt_transform = body_iface.GetWorldTransform(id);
        if (body->listener_) {
            body->listener_->OnBodyTransformUpdated(JoltMatToMat4f(jolt_transform));
        }
    }
}

void PhysicsSystem::RegisterBody(PhysicsBody* body) {
    bodies_.push_back(body);
}

void PhysicsSystem::UnregisterBody(PhysicsBody* body) {
    bodies_.erase(std::remove(bodies_.begin(), bodies_.end(), body),
                  bodies_.end());
}

}  // namespace physics
