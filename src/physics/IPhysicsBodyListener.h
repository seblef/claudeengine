#pragma once

#include "core/Mat4f.h"

namespace physics {

/// Observer notified whenever the physics system updates a body's transform.
/// Implement this in game-side objects that need to track simulation-driven movement.
class IPhysicsBodyListener {
 public:
    virtual ~IPhysicsBodyListener() = default;

    /// Called by the physics system after integrating the body for the current step.
    /// @param transform  Updated world-space transform of the body.
    virtual void OnBodyTransformUpdated(const core::Mat4f& transform) = 0;
};

}  // namespace physics
