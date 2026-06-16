#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/Mat4f.h"
#include "core/Singleton.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsBodyDesc.h"

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

class IPhysicsBodyListener;

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
    /// @param vertices      Flat float array: {x0,y0,z0, x1,y1,z1, …}.
    /// @param vertex_count  Number of vertices (array length = vertex_count * 3).
    /// @param indices       Triangle index array, three indices per triangle.
    /// @param index_count   Number of indices (= triangle_count * 3).
    ///
    /// Exact shapes enforce MotionType::Static; a non-Static motion type in desc
    /// triggers LOG_F(FATAL).
    PhysicsBody* CreateBodyWithMesh(const PhysicsBodyDesc& desc,
                                    IPhysicsBodyListener* listener,
                                    const core::Mat4f& initial_transform,
                                    const float* vertices, int vertex_count,
                                    const uint32_t* indices, int index_count);

    /// Create a static terrain body from a TerrainData heightmap.
    /// Always Static, kLayerWorld layer, no listener.
    PhysicsBody* CreateTerrainBody(const terrain::TerrainData* data,
                                   const core::Mat4f& initial_transform);

    /// Remove a body from the simulation and release its memory.
    /// Safe to call with nullptr.
    void DestroyBody(PhysicsBody* body);

 private:
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::TempAllocatorImpl>  temp_allocator_;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::JobSystemThreadPool> job_system_;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::PhysicsSystem>       jolt_system_;
    // cppcheck-suppress unusedStructMember
    std::vector<std::unique_ptr<PhysicsBody>> bodies_;  // owns all bodies
};

}  // namespace physics
