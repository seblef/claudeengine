#include "vfx/VehicleFireEffect.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include "core/Vec3f.h"
#include "game/VehicleDamage.h"
#include "particles/ParticleSystemTemplate.h"
#include "renderer/Renderer.h"
#include "vfx/VFXFire.h"
#include "vfx/VFXSystem.h"

namespace vfx {

namespace {

// The last configured damage threshold conventionally marks a full wreck
// (see physics::VehicleDamageDesc::thresholds' default {0.4, 0.6, 0.8, 1.0}),
// at which point the fire is superseded by the wreck explosion.
constexpr float kWreckThreshold = 1.f;

constexpr std::array<game::DamageZone, 5> kAllZones = {
    game::DamageZone::kFront, game::DamageZone::kRear, game::DamageZone::kLeft,
    game::DamageZone::kRight, game::DamageZone::kRoof};

}  // namespace

VehicleFireEffect::VehicleFireEffect(const physics::FireDesc& desc,
                                     const game::VehicleDamage& damage)
    : desc_(desc), damage_(&damage) {
  if (renderer::Renderer::IsInstanced()) {
    fire_template_ = particles::ParticleSystemTemplate::GetOrLoad(
        VFXFire::kFireTemplateName, renderer::Renderer::Instance().GetVideoDevice());
  }
}

VehicleFireEffect::~VehicleFireEffect() {
  Stop();
  if (fire_template_) fire_template_->Release();
}

void VehicleFireEffect::OnVehicleTransformUpdated(const core::Mat4f& world_transform) {
  last_transform_ = world_transform;
  if (!active_) return;

  active_->SetWorldTransform(world_transform);
  if (AllZonesBelowThreshold()) Stop();
}

void VehicleFireEffect::OnDamageThresholdCrossed(game::DamageZone /*zone*/, float threshold,
                                                 float /*fraction*/) {
  if (threshold >= kWreckThreshold) {
    Stop();
    return;
  }
  if (threshold >= desc_.damage_threshold) Start(last_transform_);
}

void VehicleFireEffect::Start(const core::Mat4f& world_transform) {
  if (active_) return;
  if (!VFXSystem::IsInstanced()) return;

  const core::Vec3f world_pos{world_transform(0, 3), world_transform(1, 3), world_transform(2, 3)};
  auto effect = std::make_unique<VFXFire>(fire_template_, desc_.heat_distortion);
  active_ = static_cast<VFXFire*>(
      VFXSystem::Instance().Spawn(std::move(effect), world_pos, core::Vec3f::kAxisY));
  active_->SetWorldTransform(world_transform);
}

void VehicleFireEffect::Stop() {
  if (!active_) return;
  active_->Stop();
  active_ = nullptr;
}

bool VehicleFireEffect::AllZonesBelowThreshold() const {
  return std::none_of(kAllZones.begin(), kAllZones.end(), [this](game::DamageZone zone) {
    return damage_->GetDamageFraction(zone) >= desc_.damage_threshold;
  });
}

}  // namespace vfx
