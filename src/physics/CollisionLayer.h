#pragma once

#include <cstdint>

namespace physics {

/// Collision layer indices used to categorise physics bodies.
/// The broad-phase and object-layer filter implementations reside in
/// PhysicsSystem.cpp and are never exposed to consumers.

constexpr uint16_t kLayerWorld      = 0;  ///< Static world geometry.
constexpr uint16_t kLayerPlayer     = 1;  ///< Player-controlled bodies.
constexpr uint16_t kLayerEnemy      = 2;  ///< Enemy bodies.
constexpr uint16_t kLayerProjectile = 3;  ///< Projectiles (bullets, rockets, …).

/// Total number of collision layers — keep last.
constexpr uint16_t kLayerCount      = 4;

}  // namespace physics
