#include "game/GameLight.h"

#include <cmath>

#include "core/BBox3.h"
#include "core/Mat4f.h"
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
      light_(CreateRendererLight(type, desc)),
      rest_direction_(desc.direction) {
  light_->SetCastShadow(desc.cast_shadow);
  light_->SetShadowResolution(desc.shadow_resolution);
  light_->SetShadowBias(desc.shadow_bias);
}

void GameLight::OnWorldTransformUpdated() {
  const core::Mat4f& wt = GetWorldTransform();
  light_->SetWorldMatrix(wt);
  const renderer::LightType t = light_->GetType();
  if (t == renderer::LightType::kCircleSpot) {
    const core::Vec3f d = TransformNoTranslation(rest_direction_, wt).Normalized();
    static_cast<renderer::CircleSpotLight*>(light_.get())->SetDirection(d);
  } else if (t == renderer::LightType::kRectSpot) {
    const core::Vec3f d = TransformNoTranslation(rest_direction_, wt).Normalized();
    static_cast<renderer::RectangleSpotLight*>(light_.get())->SetDirection(d);
  }
}

void GameLight::SetSpotDirection(const core::Vec3f& world_dir) {
  const renderer::LightType t = light_->GetType();
  if (t != renderer::LightType::kCircleSpot && t != renderer::LightType::kRectSpot)
    return;
  // Compute rest_direction_ = R^T * world_dir so that future rotations derive
  // the correct world-space direction.
  const core::Mat4f& wt = GetWorldTransform();
  rest_direction_ = core::Vec3f(
      wt(0,0)*world_dir.x + wt(1,0)*world_dir.y + wt(2,0)*world_dir.z,
      wt(0,1)*world_dir.x + wt(1,1)*world_dir.y + wt(2,1)*world_dir.z,
      wt(0,2)*world_dir.x + wt(1,2)*world_dir.y + wt(2,2)*world_dir.z
  ).Normalized();
  if (t == renderer::LightType::kCircleSpot)
    static_cast<renderer::CircleSpotLight*>(light_.get())->SetDirection(world_dir);
  else
    static_cast<renderer::RectangleSpotLight*>(light_.get())->SetDirection(world_dir);
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
