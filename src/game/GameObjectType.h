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
  kRoad,
  kGauge,   // editor-only scale-reference cube; ignored by game runtime
};

}  // namespace game
