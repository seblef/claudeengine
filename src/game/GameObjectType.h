#pragma once

namespace game {

// Discriminator for the concrete type of a GameObject.
enum class GameObjectType {
  kMesh,
  kLight,
  kCamera,
  kTerrain,
  kPlayerStart,
  kParticleSystem,
  kSoundEmitter,
  kPivot,
  kVehicle,
};

}  // namespace game
