#pragma once

#include <memory>
#include <vector>

#include "core/Singleton.h"
#include "physics/PhysicsBody.h"

// Forward-declare Jolt internals so Jolt headers never leak into consumers.
namespace JPH {
class PhysicsSystem;
class TempAllocatorImpl;
class JobSystemThreadPool;
}  // namespace JPH

namespace physics {

/// Singleton that owns the Jolt world, drives the simulation each frame, and
/// dispatches updated transforms to dynamic-body listeners.
///
/// Lifecycle (created by the app entry point, before GameSystem):
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
    /// to all registered Dynamic bodies.
    void Step(float dt);

    /// Register a body so Step() dispatches its transform each frame.
    /// Does not transfer ownership; caller owns the PhysicsBody.
    void RegisterBody(PhysicsBody* body);

    /// Unregister a previously registered body. Safe to call with nullptr.
    void UnregisterBody(PhysicsBody* body);

 private:
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::TempAllocatorImpl>  temp_allocator_;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::JobSystemThreadPool> job_system_;
    // cppcheck-suppress unusedStructMember
    std::unique_ptr<JPH::PhysicsSystem>       jolt_system_;
    // cppcheck-suppress unusedStructMember
    std::vector<PhysicsBody*>                 bodies_;
};

}  // namespace physics
