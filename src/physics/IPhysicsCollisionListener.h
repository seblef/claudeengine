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

    /// Called every physics step while the registered body remains in continuous
    /// contact with another body (a persisted contact, as opposed to the single
    /// OnCollision() firing when the contact first begins). Intended to drive
    /// continuous effects such as a scrape/grind VFX — must not be used to
    /// accumulate damage, since a resting or sliding contact would otherwise
    /// generate a stream of impacts every step.
    /// @param world_point        Contact point (averaged across the manifold).
    /// @param world_normal       Contact-surface normal, oriented so it points
    ///                           away from the other body and toward the
    ///                           registered body.
    /// @param relative_velocity  Velocity of the registered body relative to the
    ///                           other body at world_point (m/s), including both
    ///                           the into-surface and sliding components.
    virtual void OnSustainedContact(const core::Vec3f& world_point,
                                    const core::Vec3f& world_normal,
                                    const core::Vec3f& relative_velocity) = 0;
};

}  // namespace physics
