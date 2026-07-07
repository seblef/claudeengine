#include "vfx/VFXExplosion.h"

#include <algorithm>
#include <utility>

#include "abstract/VideoDevice.h"
#include "core/Mat4f.h"
#include "game/GameLight.h"
#include "game/GameLightDesc.h"
#include "game/GameSystem.h"
#include "particles/ParticleEmitter.h"
#include "particles/ParticleSystemTemplate.h"
#include "physics/PhysicsBody.h"
#include "physics/PhysicsSystem.h"
#include "renderer/Light.h"
#include "renderer/ParticleRenderable.h"
#include "renderer/Renderer.h"
#include "vfx/ScreenShake.h"
#include "vfx/VFXSystem.h"

namespace vfx {

VFXExplosion::VFXExplosion(const VFXExplosionDesc& desc,
                           particles::ParticleSystemTemplate* explosion_template)
    : desc_(desc), explosion_template_(explosion_template) {}

VFXExplosion::~VFXExplosion() {
  if (light_ && game::GameSystem::IsInstanced())
    game::GameSystem::Instance().RemoveObject(light_.get());

  if (renderer::Renderer::IsInstanced()) {
    for (auto& renderable : renderables_) {
      if (renderable) renderer::Renderer::Instance().RemoveRenderable(renderable.get());
    }
  }
}

void VFXExplosion::Play(const core::Vec3f& world_pos, const core::Vec3f& /*direction*/) {
  finished_      = false;
  elapsed_       = 0.f;
  light_elapsed_ = 0.f;

  if (renderer::Renderer::IsInstanced() && explosion_template_) {
    abstract::VideoDevice* video = renderer::Renderer::Instance().GetVideoDevice();
    const core::Mat4f transform = core::Mat4f::Translation(world_pos);

    sub_descs_ = explosion_template_->GetSubSystems();
    emitters_.reserve(sub_descs_.size());
    renderables_.reserve(sub_descs_.size());
    for (const auto& sub_desc : sub_descs_) {
      auto emitter = std::make_unique<particles::ParticleEmitter>(sub_desc, video);
      emitter->SetWorldTransform(transform);
      auto renderable = std::make_unique<renderer::ParticleRenderable>(
          emitter.get(), transform, /*always_visible=*/false);
      renderer::Renderer::Instance().AddRenderable(renderable.get());
      emitters_.push_back(std::move(emitter));
      renderables_.push_back(std::move(renderable));
    }
  }

  SpawnLight(world_pos);
  ApplyShockwave(world_pos);
  TriggerShake(world_pos);
}

void VFXExplosion::Update(float dt) {
  if (finished_) return;

  elapsed_ += dt;
  for (auto& emitter : emitters_) {
    emitter->Update(dt);
    emitter->UploadToGPU();
  }

  if (light_) {
    light_elapsed_ += dt;
    if (light_elapsed_ >= desc_.light_lifetime) {
      if (game::GameSystem::IsInstanced())
        game::GameSystem::Instance().RemoveObject(light_.get());
      light_.reset();
    }
  }

  if (elapsed_ >= desc_.particle_duration) finished_ = true;
}

void VFXExplosion::SpawnLight(const core::Vec3f& world_pos) {
  if (!game::GameSystem::IsInstanced()) return;

  game::GameLightDesc light_desc;
  light_desc.color       = desc_.light_color;
  light_desc.intensity   = desc_.light_intensity;
  light_desc.radius      = desc_.light_radius;
  light_desc.cast_shadow = false;  // Too short-lived to be worth a shadow map.

  light_ = std::make_unique<game::GameLight>(renderer::LightType::kOmni, light_desc);
  light_->SetWorldTransform(core::Mat4f::Translation(world_pos));
  game::GameSystem::Instance().AddObject(light_.get());
}

void VFXExplosion::ApplyShockwave(const core::Vec3f& world_pos) {
  if (!physics::PhysicsSystem::IsInstanced()) return;

  const std::vector<physics::PhysicsBody*> bodies =
      physics::PhysicsSystem::Instance().SphereOverlap(world_pos, desc_.shockwave_radius);

  for (physics::PhysicsBody* body : bodies) {
    if (!body || body->GetMotionType() != physics::MotionType::Dynamic) continue;

    const core::Mat4f transform = body->GetWorldTransform();
    const core::Vec3f body_pos{transform(0, 3), transform(1, 3), transform(2, 3)};
    const core::Vec3f offset = body_pos - world_pos;
    const float        distance = offset.Length();

    const core::Vec3f dir = distance > 1e-4f ? offset.Normalized() : core::Vec3f::kAxisY;
    const float falloff = std::clamp(1.f - distance / desc_.shockwave_radius, 0.f, 1.f);
    body->ApplyImpulse(dir * (desc_.shockwave_impulse * falloff));
  }
}

void VFXExplosion::TriggerShake(const core::Vec3f& world_pos) {
  if (!VFXSystem::IsInstanced()) return;

  auto shake = std::make_unique<ScreenShake>();
  shake->SetBaseMagnitude(desc_.shake_base_magnitude);
  shake->SetDuration(desc_.shake_duration);
  VFXSystem::Instance().Spawn(std::move(shake), world_pos, core::Vec3f::kAxisZ);
}

}  // namespace vfx
