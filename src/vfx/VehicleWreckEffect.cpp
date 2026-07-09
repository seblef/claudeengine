#include "vfx/VehicleWreckEffect.h"

#include <memory>
#include <utility>

#include <loguru.hpp>

#include "particles/ParticleSystemTemplate.h"
#include "renderer/Renderer.h"
#include "vfx/VFXExplosion.h"
#include "vfx/VFXExplosionDesc.h"
#include "vfx/VFXSystem.h"

namespace vfx {

VehicleWreckEffect::VehicleWreckEffect(const physics::WreckDesc& desc,
                                       audio::SoundManager* sound_manager,
                                       audio::ResourceManager* resource_manager)
    : desc_(desc),
      wreck_sound_(desc.wreck_sound, sound_manager, resource_manager) {
  if (renderer::Renderer::IsInstanced()) {
    explosion_template_ = particles::ParticleSystemTemplate::GetOrLoad(
        VFXExplosion::kExplosionTemplateName, renderer::Renderer::Instance().GetVideoDevice());
  }
}

VehicleWreckEffect::~VehicleWreckEffect() {
  if (explosion_template_) explosion_template_->Release();
}

void VehicleWreckEffect::OnVehicleWrecked(const core::Vec3f& world_position) {
  if (triggered_) return;
  triggered_ = true;

  if (VFXSystem::IsInstanced()) {
    auto explosion = std::make_unique<VFXExplosion>(VFXExplosionDesc{}, explosion_template_);
    VFXSystem::Instance().Spawn(std::move(explosion), world_position, core::Vec3f::kAxisY);
  }
  wreck_sound_.Trigger(world_position);

  LOG_F(INFO, "VehicleWreckEffect: wreck triggered at (%.1f, %.1f, %.1f)",
        world_position.x, world_position.y, world_position.z);
}

}  // namespace vfx
