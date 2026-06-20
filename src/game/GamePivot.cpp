#include "game/GamePivot.h"

#include "core/BBox3.h"
#include "core/Vec3f.h"
#include "game/GameObjectVisitor.h"

namespace game {

GamePivot::GamePivot()
    : GameObject(GameObjectType::kPivot,
                 core::BBox3(core::Vec3f::kZero, core::Vec3f::kZero)) {}

void GamePivot::Accept(GameObjectVisitor& visitor) {
  visitor.Visit(*this);
}

std::unique_ptr<GameObject> GamePivot::Copy(const core::Vec3f& position) const {
  auto copy = std::make_unique<GamePivot>();
  copy->SetName(GetName());
  core::Mat4f t = GetWorldTransform();
  t(0, 3) = position.x;
  t(1, 3) = position.y;
  t(2, 3) = position.z;
  copy->SetWorldTransform(t);
  return copy;
}

}  // namespace game
