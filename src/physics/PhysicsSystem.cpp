#include "physics/PhysicsSystem.h"

#include <algorithm>
#include <set>
#include <thread>
#include <utility>
#include <vector>

#include <loguru.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "core/Mat4f.h"
#include "physics/CharacterController.h"
#include "physics/CollisionLayer.h"
#include "physics/IPhysicsBodyListener.h"
#include "physics/MotionType.h"
#include "physics/PhysicsBodyDesc.h"
#include "physics/PhysicsShapeType.h"
#include "terrain/TerrainData.h"

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
core::Mat4f JoltMatToMat4f(const JPH::RMat44& m) {
    core::Mat4f result;
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            result(row, col) = m(static_cast<JPH::uint>(row),
                                 static_cast<JPH::uint>(col));
    return result;
}

/// Extracts the world position from an engine row-major transform (column 3).
JPH::RVec3 ExtractPosition(const core::Mat4f& m) {
    return JPH::RVec3(m(0, 3), m(1, 3), m(2, 3));
}

/// Extracts a rotation quaternion from the upper-left 3x3 of a row-major transform.
/// JPH::Mat44 constructor takes *column* vectors.
JPH::Quat ExtractRotation(const core::Mat4f& m) {
    JPH::Mat44 rot(
        JPH::Vec4(m(0, 0), m(1, 0), m(2, 0), 0.0f),
        JPH::Vec4(m(0, 1), m(1, 1), m(2, 1), 0.0f),
        JPH::Vec4(m(0, 2), m(1, 2), m(2, 2), 0.0f),
        JPH::Vec4(0.0f, 0.0f, 0.0f, 1.0f));
    return rot.GetQuaternion();
}

/// Maps the engine's MotionType to the Jolt equivalent.
JPH::EMotionType ToJoltMotionType(MotionType mt) {
    switch (mt) {
        case MotionType::Static:    return JPH::EMotionType::Static;
        case MotionType::Kinematic: return JPH::EMotionType::Kinematic;
        case MotionType::Dynamic:   return JPH::EMotionType::Dynamic;
    }
    return JPH::EMotionType::Static;
}

/// Fills common BodyCreationSettings fields from a PhysicsBodyDesc.
void ApplyMaterialAndLayer(const PhysicsBodyDesc& desc,
                           JPH::BodyCreationSettings& settings) {
    settings.mFriction        = desc.material.friction;
    settings.mRestitution     = desc.material.restitution;
    settings.mLinearDamping   = desc.material.linear_damping;
    settings.mAngularDamping  = desc.material.angular_damping;
    settings.mGravityFactor   = desc.material.gravity_factor;
    settings.mObjectLayer     = static_cast<JPH::ObjectLayer>(desc.collision_layer);
    settings.mMotionType      = ToJoltMotionType(desc.motion_type);

    // Override mass for kinematic/dynamic bodies so game code controls inertia.
    if (desc.motion_type != MotionType::Static) {
        settings.mOverrideMassProperties =
            JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = desc.material.mass;
    }
}

/// Extract unique edges from a triangle index buffer for debug wireframe rendering.
/// Each edge is stored as a consecutive pair of Vec3f in the output vector.
std::vector<core::Vec3f> BuildDebugEdges(const float* vertices, int vertex_count,
                                          const uint32_t* indices, int index_count) {
    (void)vertex_count;
    // Deduplicate by storing canonical (min, max) index pairs.
    std::set<std::pair<uint32_t, uint32_t>> edge_set;
    const int tri_count = index_count / 3;
    for (int t = 0; t < tri_count; ++t) {
        uint32_t i0 = indices[t * 3 + 0];
        uint32_t i1 = indices[t * 3 + 1];
        uint32_t i2 = indices[t * 3 + 2];
        edge_set.insert({std::min(i0, i1), std::max(i0, i1)});
        edge_set.insert({std::min(i1, i2), std::max(i1, i2)});
        edge_set.insert({std::min(i2, i0), std::max(i2, i0)});
    }

    std::vector<core::Vec3f> edges;
    edges.reserve(edge_set.size() * 2);
    for (const auto& [a, b] : edge_set) {
        edges.push_back({vertices[a * 3], vertices[a * 3 + 1], vertices[a * 3 + 2]});
        edges.push_back({vertices[b * 3], vertices[b * 3 + 1], vertices[b * 3 + 2]});
    }
    return edges;
}

}  // namespace

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() {
    // Destroy all bodies before tearing down the Jolt world.
    bodies_.clear();
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
    for (const auto& body : bodies_) {
        if (body->motion_type_ != MotionType::Dynamic) continue;
        const JPH::BodyID id(body->body_id_);
        if (id.IsInvalid()) continue;
        if (body->listener_) {
            body->listener_->OnBodyTransformUpdated(
                JoltMatToMat4f(body_iface.GetWorldTransform(id)));
        }
    }
}

PhysicsBody* PhysicsSystem::CreateBody(const PhysicsBodyDesc& desc,
                                       IPhysicsBodyListener* listener,
                                       const core::Mat4f& initial_transform) {
    JPH::ShapeRefC shape;
    switch (desc.shape.type) {
        case PhysicsShapeType::Box:
            shape = new JPH::BoxShape(JPH::Vec3(desc.shape.box.half_extents.x,
                                                desc.shape.box.half_extents.y,
                                                desc.shape.box.half_extents.z));
            break;
        case PhysicsShapeType::Sphere:
            shape = new JPH::SphereShape(desc.shape.sphere.radius);
            break;
        case PhysicsShapeType::Cylinder:
            shape = new JPH::CylinderShape(desc.shape.cylinder.half_height,
                                            desc.shape.cylinder.radius);
            break;
        case PhysicsShapeType::Capsule:
            shape = new JPH::CapsuleShape(desc.shape.capsule.half_height,
                                           desc.shape.capsule.radius);
            break;
        default:
            LOG_F(FATAL, "CreateBody: unsupported shape type %d for primitive body",
                  static_cast<int>(desc.shape.type));
            return nullptr;
    }

    JPH::BodyCreationSettings settings(shape,
                                       ExtractPosition(initial_transform),
                                       ExtractRotation(initial_transform),
                                       ToJoltMotionType(desc.motion_type),
                                       desc.collision_layer);
    ApplyMaterialAndLayer(desc, settings);

    const JPH::EActivation activation =
        (desc.motion_type == MotionType::Static) ? JPH::EActivation::DontActivate
                                                 : JPH::EActivation::Activate;
    const JPH::BodyID id =
        jolt_system_->GetBodyInterface().CreateAndAddBody(settings, activation);

    auto body = std::unique_ptr<PhysicsBody>(
        new PhysicsBody(id.GetIndexAndSequenceNumber(), desc.motion_type,
                        listener, jolt_system_.get()));
    PhysicsBody* result = body.get();
    bodies_.push_back(std::move(body));
    return result;
}

PhysicsBody* PhysicsSystem::CreateBodyWithMesh(const PhysicsBodyDesc& desc,
                                               IPhysicsBodyListener* listener,
                                               const core::Mat4f& initial_transform,
                                               const float* vertices,
                                               int vertex_count,
                                               const uint32_t* indices,
                                               int index_count) {
    if (desc.shape.type == PhysicsShapeType::Exact &&
        desc.motion_type != MotionType::Static) {
        LOG_F(FATAL, "CreateBodyWithMesh: Exact shapes must use MotionType::Static");
        return nullptr;
    }

    JPH::ShapeRefC shape;
    if (desc.shape.type == PhysicsShapeType::ConvexHull) {
        std::vector<JPH::Vec3> jolt_pts;
        jolt_pts.reserve(vertex_count);
        for (int i = 0; i < vertex_count; ++i) {
            jolt_pts.push_back(JPH::Vec3(vertices[i * 3],
                                         vertices[i * 3 + 1],
                                         vertices[i * 3 + 2]));
        }
        JPH::ConvexHullShapeSettings hull_settings(jolt_pts.data(),
                                                    static_cast<int>(jolt_pts.size()));
        auto result = hull_settings.Create();
        if (result.HasError()) {
            LOG_F(FATAL, "CreateBodyWithMesh: ConvexHull creation failed: %s",
                  result.GetError().c_str());
            return nullptr;
        }
        shape = result.Get();
    } else {
        // Exact triangle mesh.
        JPH::VertexList jolt_verts;
        jolt_verts.reserve(vertex_count);
        for (int i = 0; i < vertex_count; ++i) {
            jolt_verts.push_back(JPH::Float3(vertices[i * 3],
                                              vertices[i * 3 + 1],
                                              vertices[i * 3 + 2]));
        }
        JPH::IndexedTriangleList jolt_tris;
        const int tri_count = index_count / 3;
        jolt_tris.reserve(tri_count);
        for (int t = 0; t < tri_count; ++t) {
            jolt_tris.push_back(JPH::IndexedTriangle(indices[t * 3],
                                                      indices[t * 3 + 1],
                                                      indices[t * 3 + 2]));
        }
        JPH::MeshShapeSettings mesh_settings(std::move(jolt_verts),
                                              std::move(jolt_tris));
        auto result = mesh_settings.Create();
        if (result.HasError()) {
            LOG_F(FATAL, "CreateBodyWithMesh: Mesh creation failed: %s",
                  result.GetError().c_str());
            return nullptr;
        }
        shape = result.Get();
    }

    JPH::BodyCreationSettings settings(shape,
                                       ExtractPosition(initial_transform),
                                       ExtractRotation(initial_transform),
                                       ToJoltMotionType(desc.motion_type),
                                       desc.collision_layer);
    ApplyMaterialAndLayer(desc, settings);

    const JPH::EActivation activation =
        (desc.motion_type == MotionType::Static) ? JPH::EActivation::DontActivate
                                                 : JPH::EActivation::Activate;
    const JPH::BodyID id =
        jolt_system_->GetBodyInterface().CreateAndAddBody(settings, activation);

    auto body = std::unique_ptr<PhysicsBody>(
        new PhysicsBody(id.GetIndexAndSequenceNumber(), desc.motion_type,
                        listener, jolt_system_.get()));
    body->debug_edges_ = BuildDebugEdges(vertices, vertex_count, indices, index_count);

    PhysicsBody* result = body.get();
    bodies_.push_back(std::move(body));
    return result;
}

PhysicsBody* PhysicsSystem::CreateTerrainBody(const terrain::TerrainData* data,
                                              const core::Mat4f& initial_transform) {
    const int w   = data->GetTexelWidth();
    const int h   = data->GetTexelHeight();
    if (w != h) {
        LOG_F(FATAL, "CreateTerrainBody: terrain must be square (%dx%d)", w, h);
        return nullptr;
    }

    const float mpt          = data->GetMetersPerTexel();
    const float min_h        = data->GetMinHeight();
    const float max_h        = data->GetMaxHeight();
    const float range        = max_h - min_h;
    const uint16_t* samples  = data->GetRawData();
    const int n              = w * h;

    std::vector<float> heights(n);
    for (int i = 0; i < n; ++i)
        heights[i] = min_h + (samples[i] / 65535.0f) * range;

    // scale.y = 1 because sample values are already in world-space metres.
    JPH::HeightFieldShapeSettings hf_settings(
        heights.data(),
        JPH::Vec3(0.0f, 0.0f, 0.0f),
        JPH::Vec3(mpt, 1.0f, mpt),
        static_cast<JPH::uint32>(w));

    auto result = hf_settings.Create();
    if (result.HasError()) {
        LOG_F(FATAL, "CreateTerrainBody: HeightField creation failed: %s",
              result.GetError().c_str());
        return nullptr;
    }

    JPH::BodyCreationSettings settings(result.Get(),
                                       ExtractPosition(initial_transform),
                                       ExtractRotation(initial_transform),
                                       JPH::EMotionType::Static,
                                       kLayerWorld);
    settings.mFriction = 0.5f;

    const JPH::BodyID id =
        jolt_system_->GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);

    auto body = std::unique_ptr<PhysicsBody>(
        new PhysicsBody(id.GetIndexAndSequenceNumber(), MotionType::Static,
                        nullptr, jolt_system_.get()));
    PhysicsBody* terrain_body = body.get();
    bodies_.push_back(std::move(body));
    return terrain_body;
}

void PhysicsSystem::DestroyBody(PhysicsBody* body) {
    if (!body) return;

    const JPH::BodyID id(body->body_id_);
    JPH::BodyInterface& iface = jolt_system_->GetBodyInterface();
    iface.RemoveBody(id);
    iface.DestroyBody(id);

    bodies_.erase(
        std::remove_if(bodies_.begin(), bodies_.end(),
                       [body](const std::unique_ptr<PhysicsBody>& p) {
                           return p.get() == body;
                       }),
        bodies_.end());
}

std::unique_ptr<CharacterController> PhysicsSystem::CreateCharacter(
        float capsule_radius,
        float capsule_half_height,
        const core::Mat4f& initial_transform) {
    return std::unique_ptr<CharacterController>(
        new CharacterController(capsule_radius, capsule_half_height,
                                initial_transform,
                                jolt_system_.get(),
                                temp_allocator_.get()));
}

}  // namespace physics
