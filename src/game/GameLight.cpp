#include "game/GameLight.h"

#include "core/BBox3.h"
#include "core/Mat4f.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GlobalLight.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"
#include "renderer/Renderer.h"

namespace game {

// static
std::unique_ptr<renderer::Light> GameLight::CreateRendererLight(
    renderer::LightType type, const GameLightDesc& desc) {
  switch (type) {
    case renderer::LightType::kGlobal:
      return std::make_unique<renderer::GlobalLight>(
          desc.color, desc.intensity, desc.ambient_color, desc.direction);

    case renderer::LightType::kOmni:
      return std::make_unique<renderer::OmniLight>(
          desc.color, desc.intensity, desc.radius, core::Mat4f::kIdentity);

    case renderer::LightType::kCircleSpot:
      return std::make_unique<renderer::CircleSpotLight>(
          desc.color, desc.intensity,
          desc.inner_angle, desc.outer_angle, desc.range,
          desc.direction, core::Mat4f::kIdentity);

    case renderer::LightType::kRectSpot:
      return std::make_unique<renderer::RectangleSpotLight>(
          desc.color, desc.intensity,
          desc.h_angle, desc.v_angle, desc.range,
          desc.direction, core::Mat4f::kIdentity);
  }
  return nullptr;
}

GameLight::GameLight(renderer::LightType type, const GameLightDesc& desc)
    : GameObject(GameObjectType::kLight, core::BBox3{}),
      light_(CreateRendererLight(type, desc)) {}

void GameLight::OnWorldTransformUpdated() {
  light_->SetWorldMatrix(GetWorldTransform());
}

void GameLight::OnAddedToScene() {
  renderer::Renderer::Instance().AddRenderable(light_.get());
}

void GameLight::OnRemovedFromScene() {
  renderer::Renderer::Instance().RemoveRenderable(light_.get());
}

renderer::Light* GameLight::GetLight() const {
  return light_.get();
}

}  // namespace game
