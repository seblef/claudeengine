#include "game/GameLight.h"

#include <cmath>

#include "core/BBox3.h"
#include "core/Vec3f.h"
#include "renderer/CircleSpotLight.h"
#include "renderer/GlobalLight.h"
#include "renderer/OmniLight.h"
#include "renderer/RectangleSpotLight.h"
#include "renderer/Renderer.h"

namespace game {

namespace {

// Returns a local-space AABB that tightly encloses the light's wireframe.
// For pure-translation world transforms the world bbox = local_bbox + position.
core::BBox3 ComputeLocalBBox(renderer::LightType type, const GameLightDesc& desc) {
  switch (type) {
    case renderer::LightType::kGlobal:
      return {{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}};

    case renderer::LightType::kOmni: {
      const float r = desc.radius;
      return {{-r, -r, -r}, {r, r, r}};
    }

    case renderer::LightType::kCircleSpot: {
      const float base_r      = std::tan(desc.outer_angle) * desc.range;
      const core::Vec3f base_center = desc.direction * desc.range;
      // AABB of apex (origin) and base circle; expand by base_r on all axes.
      core::BBox3 bbox(core::Vec3f::kZero, core::Vec3f::kZero);
      bbox << base_center;
      const core::Vec3f exp(base_r, base_r, base_r);
      return {bbox.GetMin() - exp, bbox.GetMax() + exp};
    }

    case renderer::LightType::kRectSpot: {
      const float half_w = std::tan(desc.h_angle) * desc.range;
      const float half_h = std::tan(desc.v_angle) * desc.range;
      const core::Vec3f& dir = desc.direction;
      const core::Vec3f up =
          (std::abs(dir.y) < 0.99f) ? core::Vec3f::kAxisY : core::Vec3f::kAxisX;
      const core::Vec3f right = dir.Cross(up).Normalized();
      const core::Vec3f fwd   = dir.Cross(right);
      const core::Vec3f base_center = dir * desc.range;
      core::BBox3 bbox(core::Vec3f::kZero, core::Vec3f::kZero);
      bbox << (base_center + right * half_w + fwd * half_h);
      bbox << (base_center - right * half_w + fwd * half_h);
      bbox << (base_center - right * half_w - fwd * half_h);
      bbox << (base_center + right * half_w - fwd * half_h);
      return bbox;
    }
  }
  return {{-1.f, -1.f, -1.f}, {1.f, 1.f, 1.f}};
}

}  // namespace

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
    : GameObject(GameObjectType::kLight, ComputeLocalBBox(type, desc)),
      light_(CreateRendererLight(type, desc)) {
  light_->SetCastShadow(desc.cast_shadow);
  light_->SetShadowResolution(desc.shadow_resolution);
  light_->SetShadowBias(desc.shadow_bias);
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
  clone->SetName(GetName() + " (copy)");
  core::Mat4f t = GetWorldTransform();
  t(3, 0) = position.x;
  t(3, 1) = position.y;
  t(3, 2) = position.z;
  clone->SetWorldTransform(t);
  return clone;
}

}  // namespace game
