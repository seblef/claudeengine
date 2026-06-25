#include "game/GameGauge.h"

#include "game/GameObjectVisitor.h"

namespace game {

GameGauge::GameGauge()
    : GameObject(GameObjectType::kGauge,
                 core::BBox3(core::Vec3f(-0.5f, -0.5f, -0.5f),
                             core::Vec3f( 0.5f,  0.5f,  0.5f))) {}

void GameGauge::Accept(GameObjectVisitor& visitor) {
  visitor.Visit(*this);
}

}  // namespace game
