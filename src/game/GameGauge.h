#pragma once

#include "core/BBox3.h"
#include "core/Vec3f.h"
#include "game/GameObject.h"

namespace game {

// Editor-only scale-reference object rendered as a 1×1×1 m wireframe cube.
//
// Participates in the full editor object lifecycle (selection, transform
// gizmo, undo/redo, outliner, serialisation) exactly like any other object.
// The game runtime loads but ignores it: nothing in game/ reads kGauge objects
// after map load.
class GameGauge : public GameObject {
 public:
  GameGauge();

  void Accept(GameObjectVisitor& visitor) override;
  void OnWorldTransformUpdated() override {}
  void OnAddedToScene()          override {}
  void OnRemovedFromScene()      override {}
};

}  // namespace game
