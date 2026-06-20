#pragma once

#include "game/GameObject.h"

namespace game {

// Pure transform node with no geometry, physics, or light.
//
// Used as a logical group anchor in the scene graph: attach children to a
// GamePivot to move or rotate them as a unit without any visible representation.
// The local bounding box is degenerate (zero volume).
class GamePivot : public GameObject {
 public:
  GamePivot();

  void Accept(GameObjectVisitor& visitor) override;
  void OnWorldTransformUpdated() override {}
  void OnAddedToScene()          override {}
  void OnRemovedFromScene()      override {}

  std::unique_ptr<GameObject> Copy(const core::Vec3f& position) const override;
};

}  // namespace game
