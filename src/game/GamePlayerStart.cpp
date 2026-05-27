#include "game/GamePlayerStart.h"

#include "game/GameObjectVisitor.h"

namespace game {

GamePlayerStart::GamePlayerStart()
    : GameObject(GameObjectType::kPlayerStart, core::BBox3{}) {}

void GamePlayerStart::Accept(GameObjectVisitor& visitor) {
  visitor.Visit(*this);
}

}  // namespace game
