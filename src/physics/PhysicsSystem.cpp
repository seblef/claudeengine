#include "physics/PhysicsSystem.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <unordered_set>
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
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyManager.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/ScaledShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include "core/Mat4f.h"
#include "physics/CharacterController.h"
#include "physics/PhysicsShapeCache.h"
#include "physics/JoltDebugRenderer.h"
#include "physics/CollisionLayer.h"
#include "physics/IPhysicsBodyListener.h"
#include "physics/MotionType.h"
#include "physics/PhysicsBodyDesc.h"
#include "physics/PhysicsShapeType.h"
#include "physics/PhysicsVehicle.h"
#include "physics/RaycastResult.h"
#include "physics/VehicleDesc.h"
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
        bp_iface->MapObjectToBroadPhaseLayer(kLayerDynamic,    kBPLayerDynamic);

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
/// Normalizes each column to strip any scale before calling GetQuaternion(),
/// which requires a pure rotation matrix (unit columns).
JPH::Quat ExtractRotation(const core::Mat4f& m) {
    auto Col = [&](int c) {
        const float x = m(0, c), y = m(1, c), z = m(2, c);
        const float inv_len = 1.f / std::sqrt(x * x + y * y + z * z);
        return JPH::Vec4(x * inv_len, y * inv_len, z * inv_len, 0.f);
    };
    JPH::Mat44 rot(Col(0), Col(1), Col(2), JPH::Vec4(0.f, 0.f, 0.f, 1.f));
    return rot.GetQuaternion();
}

/// Extracts the per-axis scale from the upper-left 3x3 of a row-major transform
/// (column lengths).
core::Vec3f ExtractScale(const core::Mat4f& m) {
    auto ColLen = [&](int c) {
        const float x = m(0, c), y = m(1, c), z = m(2, c);
        return std::sqrt(x * x + y * y + z * z);
    };
    return core::Vec3f(ColLen(0), ColLen(1), ColLen(2));
}

/// Wraps shape with a RotatedTranslatedShape when offset != (0,0,0); returns
/// shape directly otherwise.
JPH::ShapeRefC ApplyCenterOffset(const JPH::ShapeRefC& shape,
                                  const core::Vec3f& offset) {
    constexpr float kZero = 0.f;
    if (std::abs(offset.x - kZero) < 1e-5f &&
        std::abs(offset.y - kZero) < 1e-5f &&
        std::abs(offset.z - kZero) < 1e-5f) {
        return shape;
    }
    JPH::RotatedTranslatedShapeSettings rts(
        JPH::Vec3(offset.x, offset.y, offset.z),
        JPH::Quat::sIdentity(),
        shape);
    auto result = rts.Create();
    if (result.HasError()) {
        LOG_F(ERROR, "ApplyCenterOffset: RotatedTranslatedShape creation failed: %s",
              result.GetError().c_str());
        return shape;
    }
    return result.Get();
}

/// Wraps base_shape with a ScaledShape when scale != (1,1,1); returns base_shape
/// directly otherwise.
JPH::ShapeRefC ApplyScale(const JPH::ShapeRefC& base_shape,
                          const core::Vec3f& scale) {
    constexpr float kUnity = 1.f;
    if (std::abs(scale.x - kUnity) < 1e-5f &&
        std::abs(scale.y - kUnity) < 1e-5f &&
        std::abs(scale.z - kUnity) < 1e-5f) {
        return base_shape;
    }
    return new JPH::ScaledShape(base_shape, JPH::Vec3(scale.x, scale.y, scale.z));
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

/// Broad-phase layer filter for raycasting.
/// Passes a bucket when at least one of its object layers is set in layer_mask.
class RaycastBroadPhaseFilter final : public JPH::BroadPhaseLayerFilter {
 public:
    explicit RaycastBroadPhaseFilter(uint16_t layer_mask) : mask_(layer_mask) {}

    bool ShouldCollide(JPH::BroadPhaseLayer in_layer) const override {
        if (in_layer == kBPLayerStatic)
            return (mask_ & (1u << kLayerWorld)) != 0u;
        // kBPLayerDynamic covers kLayerPlayer, kLayerEnemy, kLayerProjectile, kLayerDynamic.
        constexpr uint16_t kDynamicBits =
            (1u << kLayerPlayer) | (1u << kLayerEnemy) |
            (1u << kLayerProjectile) | (1u << kLayerDynamic);
        return (mask_ & kDynamicBits) != 0u;
    }

 private:
    uint16_t mask_;
};

/// Object-layer filter for raycasting.
/// Passes a body only when its layer bit is set in layer_mask.
class RaycastObjectLayerFilter final : public JPH::ObjectLayerFilter {
 public:
    explicit RaycastObjectLayerFilter(uint16_t layer_mask) : mask_(layer_mask) {}

    bool ShouldCollide(JPH::ObjectLayer in_layer) const override {
        if (in_layer >= kLayerCount) return false;
        return (mask_ & (1u << in_layer)) != 0u;
    }

 private:
    uint16_t mask_;
};

/// BodyDrawFilter that excludes terrain bodies and optionally restricts
/// rendering to a pre-computed set of body IDs.
class BodyDebugFilter final : public JPH::BodyDrawFilter {
 public:
    /// @param has_selection   True when only a subset of bodies should be drawn.
    /// @param selected_ids    Pre-computed set of JPH BodyID packed values.
    /// @param terrain_ids     Set of terrain body IDs to always skip.
    BodyDebugFilter(bool has_selection,
                    const std::unordered_set<uint32_t>& selected_ids,
                    const std::unordered_set<uint32_t>& terrain_ids)
        : has_selection_(has_selection),
          selected_ids_(selected_ids),
          terrain_ids_(terrain_ids) {}

    bool ShouldDraw(const JPH::Body& inBody) const override {
        const uint32_t id = inBody.GetID().GetIndexAndSequenceNumber();
        if (terrain_ids_.count(id)) return false;
        if (has_selection_)
            return selected_ids_.count(id) > 0;
        return true;
    }

 private:
    bool has_selection_;
    const std::unordered_set<uint32_t>& selected_ids_;
    const std::unordered_set<uint32_t>& terrain_ids_;
};

}  // namespace

PhysicsSystem::PhysicsSystem() = default;

PhysicsSystem::~PhysicsSystem() {
    // Release cached mesh shapes (heap-allocated JPH::ShapeRefC*) before
    // destroying bodies; each body still holds its own ref via base_shape_.
    for (auto& [key, ptr] : mesh_shape_cache_)
        delete static_cast<JPH::ShapeRefC*>(ptr);
    mesh_shape_cache_.clear();

    // Destroy vehicles (constraints + bodies) before regular bodies and the world.
    vehicles_.clear();
    // Destroy all bodies before tearing down the Jolt world.
    bodies_.clear();
    jolt_system_.reset();
    job_system_.reset();
    temp_allocator_.reset();
    debug_renderer_.reset();  // ~DebugRenderer() clears sInstance
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void PhysicsSystem::Init() {
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    constexpr int kTempAllocatorSizeBytes = 16 * 1024 * 1024;  // 16 MB
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

    debug_renderer_ = std::make_unique<JoltDebugRenderer>();
    JPH::DebugRenderer::sInstance = debug_renderer_.get();
}

void PhysicsSystem::SetShapeCacheDir(std::string_view path) {
    shape_cache_dir_ = std::string(path);
}

void PhysicsSystem::Step(float dt) {
    jolt_system_->Update(dt, 1, temp_allocator_.get(), job_system_.get());

    for (const auto& body : bodies_) {
        if (body->motion_type_ != MotionType::Dynamic) continue;
        const JPH::BodyID id(body->body_id_);
        if (id.IsInvalid()) continue;
        if (body->listener_)
            body->listener_->OnBodyTransformUpdated(body->GetWorldTransform());
    }

    for (const auto& vehicle : vehicles_) {
        if (vehicle->listener_)
            vehicle->listener_->OnBodyTransformUpdated(
                vehicle->GetBodyWorldTransform());
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

    shape = ApplyCenterOffset(shape, desc.shape.center_offset);

    const core::Vec3f scale = ExtractScale(initial_transform);
    JPH::BodyCreationSettings settings(ApplyScale(shape, scale),
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
                        listener, jolt_system_.get(), shape.GetPtr(), scale));
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
                                               int index_count,
                                               const void* shape_cache_key) {
    if (desc.shape.type == PhysicsShapeType::Exact &&
        desc.motion_type != MotionType::Static) {
        LOG_F(FATAL, "CreateBodyWithMesh: Exact shapes must use MotionType::Static");
        return nullptr;
    }

    JPH::ShapeRefC shape;

    // Reuse an already-built Jolt shape for this mesh template if available.
    if (shape_cache_key) {
        const auto it = mesh_shape_cache_.find(shape_cache_key);
        if (it != mesh_shape_cache_.end())
            shape = *static_cast<JPH::ShapeRefC*>(it->second);
    }

    // Disk cache (L2): try to deserialise when there is a session-cache miss.
    std::string disk_cache_path;
    if (shape.GetPtr() == nullptr && !shape_cache_dir_.empty()) {
        const uint64_t hash = HashBytes(vertices, vertex_count * 3 * sizeof(float));
        const uint64_t hash2 = HashBytes(indices,  index_count  * sizeof(uint32_t));
        // Combine both hashes with FNV-1a to produce a single key.
        uint64_t combined = hash;
        const auto* h2p = reinterpret_cast<const uint8_t*>(&hash2);
        for (size_t i = 0; i < sizeof(hash2); ++i) {
            combined ^= h2p[i];
            combined *= 1099511628211ULL;
        }
        disk_cache_path = ShapeCachePath(shape_cache_dir_, "meshes", combined);
        shape           = TryLoadShape(disk_cache_path);
        if (shape.GetPtr() != nullptr && shape_cache_key)
            mesh_shape_cache_[shape_cache_key] = new JPH::ShapeRefC(shape);
    }

    if (shape.GetPtr() == nullptr) {
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

        if (shape_cache_key)
            mesh_shape_cache_[shape_cache_key] = new JPH::ShapeRefC(shape);

        if (!disk_cache_path.empty())
            SaveShape(shape, disk_cache_path, /*cleanup_siblings=*/false);
    }

    const core::Vec3f scale = ExtractScale(initial_transform);
    JPH::BodyCreationSettings settings(ApplyScale(shape, scale),
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
                        listener, jolt_system_.get(), shape.GetPtr(), scale));

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

    // Disk cache (L2): try to deserialise a previously built HeightFieldShape.
    JPH::ShapeRefC terrain_shape;
    std::string    cache_path;
    if (!shape_cache_dir_.empty()) {
        const uint64_t hash = HashBytes(samples, n * sizeof(uint16_t));
        cache_path    = ShapeCachePath(shape_cache_dir_, "terrain", hash);
        terrain_shape = TryLoadShape(cache_path);
    }

    if (terrain_shape.GetPtr() == nullptr) {
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
        terrain_shape = result.Get();

        if (!cache_path.empty())
            SaveShape(terrain_shape, cache_path, /*cleanup_siblings=*/true);
    }

    JPH::BodyCreationSettings settings(terrain_shape,
                                       ExtractPosition(initial_transform),
                                       ExtractRotation(initial_transform),
                                       JPH::EMotionType::Static,
                                       kLayerWorld);
    settings.mFriction = 0.7f;

    const JPH::BodyID id =
        jolt_system_->GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::DontActivate);

    auto body = std::unique_ptr<PhysicsBody>(
        new PhysicsBody(id.GetIndexAndSequenceNumber(), MotionType::Static,
                        nullptr, jolt_system_.get(),
                        nullptr, core::Vec3f(1.f, 1.f, 1.f)));
    PhysicsBody* terrain_body = body.get();
    terrain_body_ids_.insert(id.GetIndexAndSequenceNumber());
    bodies_.push_back(std::move(body));
    return terrain_body;
}

void PhysicsSystem::DestroyBody(PhysicsBody* body) {
    if (!body) return;

    const JPH::BodyID id(body->body_id_);
    JPH::BodyInterface& iface = jolt_system_->GetBodyInterface();
    iface.RemoveBody(id);
    iface.DestroyBody(id);
    terrain_body_ids_.erase(id.GetIndexAndSequenceNumber());

    bodies_.erase(
        std::remove_if(bodies_.begin(), bodies_.end(),
                       [body](const std::unique_ptr<PhysicsBody>& p) {
                           return p.get() == body;
                       }),
        bodies_.end());
}

namespace {

JPH::Ref<JPH::WheelSettings> BuildWheelSettings(const WheelDesc& wd,
                                                  const WheelGeometry& geo,
                                                  float brake_torque,
                                                  float handbrake_torque,
                                                  float max_steer_angle,
                                                  bool apply_handbrake) {
    auto* ws = new JPH::WheelSettingsWV();
    ws->mPosition          = JPH::Vec3(wd.position.x, wd.position.y, wd.position.z);
    ws->mRadius            = geo.radius;
    ws->mWidth             = geo.width;
    ws->mSuspensionMinLength = wd.suspension_min_length;
    ws->mSuspensionMaxLength = wd.suspension_max_length;
    ws->mSuspensionSpring  = JPH::SpringSettings(
        JPH::ESpringMode::StiffnessAndDamping,
        wd.suspension_stiffness,
        wd.suspension_damping);
    ws->mMaxSteerAngle     = wd.is_steered ? max_steer_angle : 0.f;
    ws->mMaxBrakeTorque    = brake_torque;
    ws->mMaxHandBrakeTorque = apply_handbrake ? handbrake_torque : 0.f;
    return ws;
}

}  // namespace

PhysicsVehicle* PhysicsSystem::CreateVehicle(const VehicleDesc& desc,
                                              IPhysicsBodyListener* listener,
                                              const core::Mat4f& initial_transform,
                                              const WheelGeometry& front_wheel_geo,
                                              const WheelGeometry& rear_wheel_geo,
                                              const core::Vec3f* body_vertices,
                                              int body_vertex_count) {
    // ---- Body shape (box or convex hull, + optional COM offset) -------------
    JPH::ShapeRefC base_shape;
    if (body_vertices != nullptr && body_vertex_count > 0) {
        std::vector<JPH::Vec3> jolt_pts;
        jolt_pts.reserve(body_vertex_count);
        for (int i = 0; i < body_vertex_count; ++i)
            jolt_pts.push_back(JPH::Vec3(body_vertices[i].x,
                                          body_vertices[i].y,
                                          body_vertices[i].z));
        JPH::ConvexHullShapeSettings hull_settings(jolt_pts.data(),
                                                    static_cast<int>(jolt_pts.size()));
        auto result = hull_settings.Create();
        if (result.HasError()) {
            LOG_F(ERROR, "CreateVehicle: ConvexHull shape failed: %s",
                  result.GetError().c_str());
            return nullptr;
        }
        base_shape = result.Get();
    } else {
        base_shape = new JPH::BoxShape(JPH::Vec3(desc.half_extents.x,
                                                  desc.half_extents.y,
                                                  desc.half_extents.z));
    }

    JPH::ShapeRefC shape;
    constexpr float kZero = 0.f;
    const bool has_com_offset =
        std::abs(desc.com_offset.x - kZero) > 1e-5f ||
        std::abs(desc.com_offset.y - kZero) > 1e-5f ||
        std::abs(desc.com_offset.z - kZero) > 1e-5f;
    if (has_com_offset) {
        JPH::OffsetCenterOfMassShapeSettings ocm(
            JPH::Vec3(desc.com_offset.x, desc.com_offset.y, desc.com_offset.z),
            base_shape);
        auto result = ocm.Create();
        if (result.HasError()) {
            LOG_F(ERROR, "CreateVehicle: OffsetCOM shape failed: %s",
                  result.GetError().c_str());
            return nullptr;
        }
        shape = result.Get();
    } else {
        shape = base_shape;
    }

    // ---- Body ----------------------------------------------------------------
    JPH::BodyCreationSettings body_settings(shape,
                                             ExtractPosition(initial_transform),
                                             ExtractRotation(initial_transform),
                                             JPH::EMotionType::Dynamic,
                                             kLayerDynamic);
    body_settings.mOverrideMassProperties =
        JPH::EOverrideMassProperties::CalculateInertia;
    body_settings.mMassPropertiesOverride.mMass = desc.mass;

    JPH::BodyInterface& iface = jolt_system_->GetBodyInterface();
    JPH::Body* body = iface.CreateBody(body_settings);
    if (!body) {
        LOG_F(ERROR, "CreateVehicle: failed to create vehicle body");
        return nullptr;
    }
    iface.AddBody(body->GetID(), JPH::EActivation::Activate);

    // ---- Wheel settings (indices: 0=FL, 1=FR, 2=RL, 3=RR) ------------------
    JPH::VehicleConstraintSettings vc_settings;
    vc_settings.mWheels.push_back(BuildWheelSettings(
        desc.front_left,  front_wheel_geo, desc.brake_torque, desc.handbrake_torque,
        desc.max_steer_angle, false));
    vc_settings.mWheels.push_back(BuildWheelSettings(
        desc.front_right, front_wheel_geo, desc.brake_torque, desc.handbrake_torque,
        desc.max_steer_angle, false));
    vc_settings.mWheels.push_back(BuildWheelSettings(
        desc.rear_left,   rear_wheel_geo, desc.brake_torque, desc.handbrake_torque,
        desc.max_steer_angle, true));
    vc_settings.mWheels.push_back(BuildWheelSettings(
        desc.rear_right,  rear_wheel_geo, desc.brake_torque, desc.handbrake_torque,
        desc.max_steer_angle, true));

    // ---- Controller + differentials -----------------------------------------
    auto* ctrl = new JPH::WheeledVehicleControllerSettings();
    ctrl->mEngine.mMaxTorque            = desc.max_engine_torque;
    ctrl->mEngine.mInertia              = desc.engine_inertia;
    ctrl->mTransmission.mSwitchTime     = desc.gear_switch_time;
    ctrl->mTransmission.mClutchStrength = desc.clutch_strength;

    const bool front_driven =
        desc.front_left.is_driven || desc.front_right.is_driven;
    const bool rear_driven =
        desc.rear_left.is_driven || desc.rear_right.is_driven;
    const int num_diffs = (front_driven ? 1 : 0) + (rear_driven ? 1 : 0);
    const float torque_per_diff =
        num_diffs > 0 ? 1.f / static_cast<float>(num_diffs) : 1.f;

    if (front_driven) {
        JPH::VehicleDifferentialSettings diff;
        diff.mLeftWheel         = 0;
        diff.mRightWheel        = 1;
        diff.mEngineTorqueRatio = torque_per_diff;
        ctrl->mDifferentials.push_back(diff);
    }
    if (rear_driven) {
        JPH::VehicleDifferentialSettings diff;
        diff.mLeftWheel         = 2;
        diff.mRightWheel        = 3;
        diff.mEngineTorqueRatio = torque_per_diff;
        ctrl->mDifferentials.push_back(diff);
    }
    vc_settings.mController = ctrl;

    // ---- Constraint ----------------------------------------------------------
    JPH::VehicleConstraint* constraint =
        new JPH::VehicleConstraint(*body, vc_settings);
    // Use kLayerDynamic so GetDefaultLayerFilter includes kLayerWorld (terrain).
    // kLayerWorld as tester layer would query "what collides with kLayerWorld",
    // which excludes kLayerWorld itself — making wheel raycasts miss the terrain.
    constraint->SetVehicleCollisionTester(
        new JPH::VehicleCollisionTesterRay(kLayerDynamic));

    jolt_system_->AddConstraint(constraint);
    jolt_system_->AddStepListener(constraint);

    auto vehicle = std::unique_ptr<PhysicsVehicle>(
        new PhysicsVehicle(body, constraint, jolt_system_.get(), listener));
    PhysicsVehicle* result = vehicle.get();
    vehicles_.push_back(std::move(vehicle));
    return result;
}

void PhysicsSystem::DestroyVehicle(PhysicsVehicle* vehicle) {
    if (!vehicle) return;
    vehicles_.erase(
        std::remove_if(vehicles_.begin(), vehicles_.end(),
                       [vehicle](const std::unique_ptr<PhysicsVehicle>& p) {
                           return p.get() == vehicle;
                       }),
        vehicles_.end());
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

std::optional<RaycastResult> PhysicsSystem::Raycast(
        const core::Vec3f& origin,
        const core::Vec3f& direction,
        float max_dist,
        uint16_t layer_mask) const {
    const JPH::RVec3 jolt_origin(origin.x, origin.y, origin.z);
    const JPH::Vec3  jolt_dir(direction.x * max_dist,
                               direction.y * max_dist,
                               direction.z * max_dist);
    const JPH::RRayCast ray{ jolt_origin, jolt_dir };
    JPH::RayCastResult  hit;

    RaycastBroadPhaseFilter  broad_filter(layer_mask);
    RaycastObjectLayerFilter obj_filter(layer_mask);

    if (!jolt_system_->GetNarrowPhaseQuery().CastRay(ray, hit, broad_filter,
                                                      obj_filter))
        return std::nullopt;

    RaycastResult result;
    result.distance = hit.mFraction * max_dist;
    result.position = origin + direction * result.distance;

    // Retrieve surface normal by locking the hit body for reading.
    const JPH::RVec3 jolt_hit_pos(result.position.x, result.position.y,
                                   result.position.z);
    {
        JPH::BodyLockRead lock(jolt_system_->GetBodyLockInterface(), hit.mBodyID);
        if (lock.Succeeded()) {
            const JPH::Vec3 n = lock.GetBody().GetWorldSpaceSurfaceNormal(
                hit.mSubShapeID2, jolt_hit_pos);
            result.normal = { n.GetX(), n.GetY(), n.GetZ() };
        }
    }

    // Resolve BodyID to an engine PhysicsBody* via linear scan.
    const uint32_t target_id = hit.mBodyID.GetIndexAndSequenceNumber();
    const auto it = std::find_if(bodies_.begin(), bodies_.end(),
        [target_id](const std::unique_ptr<PhysicsBody>& b) {
            return b->body_id_ == target_id;
        });
    result.body = (it != bodies_.end()) ? it->get() : nullptr;

    return result;
}

void PhysicsSystem::DrawDebug(const PhysicsDebugDrawSettings& settings) {
    // Build a lookup set from the selected bodies (uses body_id_ via friend access).
    std::unordered_set<uint32_t> selected_ids;
    if (settings.selectedBodies) {
        for (const PhysicsBody* b : *settings.selectedBodies)
            selected_ids.insert(b->body_id_);
    }

    BodyDebugFilter filter(settings.selectedBodies != nullptr, selected_ids,
                           terrain_body_ids_);

    if (settings.drawShapes) {
        JPH::BodyManager::DrawSettings draw_settings;
        draw_settings.mDrawShape = true;
        draw_settings.mDrawShapeWireframe = true;
        jolt_system_->DrawBodies(draw_settings, debug_renderer_.get(), &filter);
    }

    if (settings.drawConstraints)
        jolt_system_->DrawConstraints(debug_renderer_.get());

    // Contact points are rendered by the physics solver during Step(); the flag
    // is set here so it takes effect on the next simulation tick.
    JPH::ContactConstraintManager::sDrawContactPoint = settings.drawContactPoints;

    if (settings.drawBroadPhase)
        debug_renderer_->DrawWireBox(
            jolt_system_->GetBroadPhaseQuery().GetBounds(),
            JPH::Color::sGreen);
}

}  // namespace physics
