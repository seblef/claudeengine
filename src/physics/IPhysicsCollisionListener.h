#pragma once

#include "core/Vec3f.h"

namespace physics {

/// Observer notified when a body registered for collision events takes part in
/// a new physics contact. Unlike IPhysicsBodyListener (per-step transform sync),
/// this fires only when a contact begins — i.e. an actual impact, not resting
/// contact — making it suitable for damage, impact SFX, and VFX triggers.
class IPhysicsCollisionListener {
 public:
    virtual ~IPhysicsCollisionListener() = default;

    /// Called once when a new contact is detected involving the registered body.
    /// @param world_point  World-space point of impact (averaged across the
    ///                     contact manifold when it has multiple points).
    /// @param impulse      Estimated total contact impulse magnitude (kg·m/s),
    ///                     from Jolt's EstimateCollisionResponse.
    virtual void OnCollision(const core::Vec3f& world_point, float impulse) = 0;
};

}  // namespace physics
