#include "game/GameLight.h"

#include "core/BBox3.h"
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
    : GameObject(GameObjectType::kLight,
                 core::BBox3({-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f})),
      light_(CreateRendererLight(type, desc)) {
  light_->SetCastShadow(desc.cast_shadow);
  light_->SetShadowResolution(desc.shadow_resolution);
  light_->SetShadowBias(desc.shadow_bias);
  light_->SetGizmoKey(this);
  RefreshBBox();
}

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

void GameLight::RefreshBBox() {
  SetLocalBBox(light_->GetLocalBBox());
}

std::unique_ptr<game::GameObject> GameLight::Copy(const core::Vec3f& position) const {
  const renderer::Light* l = light_.get();
  GameLightDesc desc;
  desc.color             = l->GetColor();
  desc.intensity         = l->GetIntensity();
  desc.cast_shadow       = l->GetCastShadow();
  desc.shadow_resolution = l->GetShadowResolution();
  desc.shadow_bias       = l->GetShadowBias();

  switch (l->GetType()) {
    case renderer::LightType::kOmni:
      desc.radius = static_cast<const renderer::OmniLight*>(l)->GetRadius();
      break;
    case renderer::LightType::kCircleSpot: {
      const auto* s = static_cast<const renderer::CircleSpotLight*>(l);
      desc.inner_angle = s->GetInnerAngle();
      desc.outer_angle = s->GetOuterAngle();
      desc.range       = s->GetRange();
      desc.direction   = s->GetDirection();
      break;
    }
    case renderer::LightType::kRectSpot: {
      const auto* r = static_cast<const renderer::RectangleSpotLight*>(l);
      desc.h_angle   = r->GetHAngle();
      desc.v_angle   = r->GetVAngle();
      desc.range     = r->GetRange();
      desc.direction = r->GetDirection();
      break;
    }
    case renderer::LightType::kGlobal: {
      const auto* g = static_cast<const renderer::GlobalLight*>(l);
      desc.direction     = g->GetDirection();
      desc.ambient_color = g->GetAmbientColor();
      break;
    }
  }

  auto clone = std::make_unique<GameLight>(l->GetType(), desc);
  clone->SetName(GetName());
  core::Mat4f t = GetWorldTransform();
  t(0, 3) = position.x;
  t(1, 3) = position.y;
  t(2, 3) = position.z;
  clone->SetWorldTransform(t);
  return clone;
}

}  // namespace game
