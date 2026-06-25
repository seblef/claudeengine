#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "core/Vec3f.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsBodyDesc.h"
#include "physics/PhysicsVehicle.h"
#include "physics/RaycastResult.h"
#include "physics/SurfaceMaterial.h"
#include "physics/VehicleDesc.h"

// Forward-declare Jolt internals so Jolt headers never leak into consumers.
namespace JPH {
class PhysicsSystem;
class TempAllocatorImpl;
class JobSystemThreadPool;
}  // namespace JPH

namespace terrain {
class TerrainData;
}  // namespace terrain

namespace physics {

class CharacterController;
class IPhysicsBodyListener;
class JoltDebugRenderer;

/// Controls which debug geometry PhysicsSystem::DrawDebug() renders each frame.
struct PhysicsDebugDrawSettings {
    /// When non-null, only these bodies are drawn.
    /// When null, all non-terrain bodies are drawn.
    const std::vector<const PhysicsBody*>* selectedBodies = nullptr;

    bool drawShapes        = false;  ///< Draw body shape wireframes.
    bool drawConstraints   = false;  ///< Draw constraint joints.
    bool drawContactPoints = false;  ///< Draw contact manifolds (affects next Step).
    bool drawBroadPhase    = false;  ///< Draw the broadphase world AABB.
};

/// Singleton that owns the Jolt world, all bodies, and drives the simulation.
///
/// Lifecycle (constructed by the app entry point, before GameSystem):
///   new PhysicsSystem();
///   PhysicsSystem::Instance().Init();
///   // per frame:
///   PhysicsSystem::Instance().Step(dt);
///   // teardown (before GameSystem teardown):
///   PhysicsSystem::Shutdown();
class PhysicsSystem : public core::Singleton<PhysicsSystem> {
 public:
    PhysicsSystem();
    ~PhysicsSystem();

    /// Initialise the Jolt world.  Must be called once before Step().
    void Init();

    /// Set the directory where serialised Jolt shape cache files are stored.
    /// Call after Init() and before any CreateTerrainBody / CreateBodyWithMesh
    /// calls.  If never called (or path is empty) disk caching is disabled.
    void SetShapeCacheDir(std::string_view path);

    /// Advance the simulation by dt seconds, then dispatch updated transforms
    /// to all registered Dynamic body listeners.
    void Step(float dt);

    // ---- Body factory -------------------------------------------------------

    /// Create a body from a primitive shape description (Box, Sphere, Cylinder,
    /// Capsule, or Terrain shape type).
    /// @param listener  Optional observer for transform updates (Dynamic bodies).
    ///                  May be nullptr.
    /// @returns         Non-owning pointer; lifetime is managed by this system.
    PhysicsBody* CreateBody(const PhysicsBodyDesc& desc,
                            IPhysicsBodyListener* listener,
                            const core::Mat4f& initial_transform);

    /// Create a body backed by a ConvexHull or Exact triangle-mesh shape.
    /// @param vertices        Flat float array: {x0,y0,z0, x1,y1,z1, …}.
    /// @param vertex_count    Number of vertices (array length = vertex_count * 3).
    /// @param indices         Triangle index array, three indices per triangle.
    /// @param index_count     Number of indices (= triangle_count * 3).
    /// @param shape_cache_key Opaque pointer used as a cache key so that all
    ///                        instances sharing the same geometry (e.g. the same
    ///                        MeshTemplate*) reuse a single Jolt shape instead of
    ///                        rebuilding it per instance.  Pass nullptr to skip
    ///                        caching.  Callers must use a consistent shape type
    ///                        (ConvexHull vs Exact) for the same key.
    ///
    /// Exact shapes enforce MotionType::Static; a non-Static motion type in desc
    /// triggers LOG_F(FATAL).
    PhysicsBody* CreateBodyWithMesh(const PhysicsBodyDesc& desc,
                                    IPhysicsBodyListener* listener,
                                    const core::Mat4f& initial_transform,
                                    const float* vertices, int vertex_count,
                                    const uint32_t* indices, int index_count,
                                    const void* shape_cache_key = nullptr);

    /// Create a static terrain body from a TerrainData heightmap.
    /// Always Static, kLayerWorld layer, no listener.
    PhysicsBody* CreateTerrainBody(const terrain::TerrainData* data,
                                   const core::Mat4f& initial_transform);

    /// Remove a body from the simulation and release its memory.
    /// Safe to call with nullptr.
    void DestroyBody(PhysicsBody* body);

    // ---- Character factory ---------------------------------------------------

    // ---- Vehicle factory ----------------------------------------------------

    /// Create a wheeled vehicle from a descriptor.
    ///
    /// @param desc              Vehicle physics descriptor (mass, extents, torques, wheels).
    /// @param listener          Optional observer receiving body transform updates.  May be nullptr.
    /// @param initial_transform World-space initial transform for the vehicle body.
    /// @param front_wheel_geo   Wheel radius and width for the front axle (inferred from mesh bbox).
    /// @param rear_wheel_geo    Wheel radius and width for the rear axle (inferred from mesh bbox).
    /// @param body_vertices     Optional CPU positions of the body mesh for a ConvexHull body shape.
    ///                          Pass nullptr (default) to use a box shape from desc.half_extents.
    /// @param body_vertex_count Number of vertices in body_vertices (ignored when nullptr).
    /// @returns                 Non-owning pointer; lifetime is managed by this system.
    PhysicsVehicle* CreateVehicle(const VehicleDesc& desc,
                                   IPhysicsBodyListener* listener,
                                   const core::Mat4f& initial_transform,
                                   const WheelGeometry& front_wheel_geo,
                                   const WheelGeometry& rear_wheel_geo,
                                   const core::Vec3f* body_vertices = nullptr,
                                   int                body_vertex_count = 0);

    /// Remove a vehicle from the simulation and release its memory.
    /// Safe to call with nullptr.
    void DestroyVehicle(PhysicsVehicle* vehicle);

    // ---- Character factory ---------------------------------------------------

    /// Create a kinematic character capsule using Jolt CharacterVirtual.
    /// The caller owns the returned pointer; destroying it removes the character.
    /// @param capsule_radius      Radius of the capsule hemisphere (metres).
    /// @param capsule_half_height Half the height of the cylinder portion (metres).
    /// @param initial_transform   World-space transform for the capsule base centre.
    /// @returns                   Owning unique_ptr; reset it to destroy the character.
    std::unique_ptr<CharacterController> CreateCharacter(
        float capsule_radius,
        float capsule_half_height,
        const core::Mat4f& initial_transform);

    // ---- Debug rendering ----------------------------------------------------

    /// Trigger Jolt's built-in debug draw passes.  Must be called each frame
    /// (typically after Step()) to keep contact-point flags in sync.
    void DrawDebug(const PhysicsDebugDrawSettings& settings);

    // ---- Surface type tagging -----------------------------------------------

    /// Tag a body's surface type for track-texture selection.
    /// @param body  A non-null body previously returned by CreateBody* or CreateTerrainBody.
    /// @param type  Surface category to associate.  Overrides any previous tag.
    void SetBodySurfaceType(const PhysicsBody* body, SurfaceType type);

    /// Returns the surface type associated with the given raw Jolt body ID,
    /// or SurfaceType::kGeneric if the body was never tagged.
    [[nodiscard]] SurfaceType GetSurfaceType(uint32_t jolt_body_id) const;

    // ---- Query --------------------------------------------------------------

    /// Cast a ray and return the closest hit, or std::nullopt if nothing is hit.
    ///
    /// The tested segment is origin + direction * [0, max_dist].
    ///
    /// @param layer_mask  Bitmask of kLayer* constants; only bodies whose
    ///                    collision_layer bit is set are tested.
    [[nodiscard]] std::optional<RaycastResult> Raycast(
        const core::Vec3f& origin,
        const core::Vec3f& direction,
        float max_dist,
        uint16_t layer_mask = 0xFFFF) const;

 private:
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::TempAllocatorImpl>  temp_allocator_;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::JobSystemThreadPool> job_system_;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::PhysicsSystem>       jolt_system_;
    // cppcheck-suppress unusedStructMember
    std::vector<std::unique_ptr<PhysicsBody>>    bodies_;    // owns all bodies
    // cppcheck-suppress unusedStructMember
    std::vector<std::unique_ptr<PhysicsVehicle>> vehicles_;  // owns all vehicles
    // Maps Jolt body ID (index+sequence packed value) to surface type.
    // Terrain bodies are auto-registered as kTerrain; road bodies are tagged
    // by callers via SetBodySurfaceType(); all others default to kGeneric.
    // cppcheck-suppress unusedStructMember
    std::unordered_map<uint32_t, SurfaceType>  surface_types_;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JoltDebugRenderer> debug_renderer_;
    // Maps opaque shape_cache_key pointers to heap-allocated JPH::ShapeRefC*
    // so each unique mesh geometry builds its Jolt shape only once.
    // Values are void* to avoid exposing JPH types in this header.
    // cppcheck-suppress unusedStructMember
    std::unordered_map<const void*, void*>    mesh_shape_cache_;
    // cppcheck-suppress unusedStructMember
    std::string shape_cache_dir_;  ///< Empty = disk caching disabled.
};

}  // namespace physics
