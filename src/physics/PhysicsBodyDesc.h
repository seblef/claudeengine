#pragma once

#include <cstdint>

#include "physics/CollisionLayer.h"
#include "physics/MotionType.h"
#include "physics/PhysicsMaterialDesc.h"
#include "physics/PhysicsShapeDesc.h"

namespace physics {

/// Full description of a physics body passed to the physics system at creation time.
struct PhysicsBodyDesc {
    // cppcheck-suppress unusedStructMember
    PhysicsShapeDesc    shape;                          ///< Shape and its parameters.
    // cppcheck-suppress unusedStructMember
    PhysicsMaterialDesc material;                       ///< Surface and mass properties.
    // cppcheck-suppress unusedStructMember
    MotionType          motion_type     = MotionType::Static;  ///< Simulation mode.
    // cppcheck-suppress unusedStructMember
    uint16_t            collision_layer = kLayerWorld;  ///< Layer this body belongs to.
    // cppcheck-suppress unusedStructMember
    uint16_t            collision_mask  = 0xFFFF;       ///< Bitmask of layers this body collides with.

    [[nodiscard]] bool operator==(const PhysicsBodyDesc& o) const {
        return shape           == o.shape
            && material        == o.material
            && motion_type     == o.motion_type
            && collision_layer == o.collision_layer
            && collision_mask  == o.collision_mask;
    }
    [[nodiscard]] bool operator!=(const PhysicsBodyDesc& o) const {
        return !(*this == o);
    }
};

}  // namespace physics
