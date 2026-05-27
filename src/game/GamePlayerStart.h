#pragma once

#include "core/BBox3.h"
#include "game/GameObject.h"

namespace game {

// Marks the spawn position for the FPS camera when a map is loaded in game mode.
//
// This object has no visual representation at runtime. GameSystem scans for the
// first GamePlayerStart after map loading and calls
// FPSCameraController::SetPosition() with its world-space translation.
class GamePlayerStart : public GameObject {
 public:
  GamePlayerStart();

  void Accept(GameObjectVisitor& visitor) override;
  void OnWorldTransformUpdated() override {}
  void OnAddedToScene()          override {}
  void OnRemovedFromScene()      override {}
};

}  // namespace game
