#pragma once

namespace game {

// Discriminator for the concrete type of a GameObject.
enum class GameObjectType {
  kMesh,
  kLight,
  kCamera,
};

}  // namespace game
