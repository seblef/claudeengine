#pragma once

#include <cstdint>

namespace physics {

/// Collision layer indices used to categorise physics bodies.
/// The broad-phase and object-layer filter implementations reside in
/// PhysicsSystem.cpp and are never exposed to consumers.

constexpr uint16_t kLayerWorld      = 0;  ///< Static world geometry (terrain, static meshes).
constexpr uint16_t kLayerPlayer     = 1;  ///< Player-controlled bodies.
constexpr uint16_t kLayerEnemy      = 2;  ///< Enemy bodies.
constexpr uint16_t kLayerProjectile = 3;  ///< Projectiles (bullets, rockets, …).
constexpr uint16_t kLayerDynamic    = 4;  ///< Generic dynamic physics objects (crates, barrels, debris).

/// Total number of collision layers — keep last.
constexpr uint16_t kLayerCount      = 5;

}  // namespace physics
