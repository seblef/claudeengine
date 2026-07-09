#include "vfx/VehicleFireEffect.h"

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

#include <loguru.hpp>

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

void VehicleFireEffect::OnVehicleTransformUpdated(float dt, const core::Mat4f& world_transform) {
  last_transform_ = world_transform;
  if (!active_) return;

  active_time_ += dt;
  active_->SetWorldTransform(world_transform);

  const bool stop_requested = wreck_pending_ || AllZonesBelowThreshold();
  if (stop_requested && active_time_ >= desc_.min_burn_time) Stop();
}

void VehicleFireEffect::OnDamageThresholdCrossed(game::DamageZone /*zone*/, float threshold,
                                                 float /*fraction*/) {
  if (threshold >= kWreckThreshold) {
    if (!wreck_pending_) {
      wreck_pending_ = true;
      LOG_F(INFO, "VehicleFireEffect: wreck threshold reached, fire stop deferred "
                  "until min_burn_time (%.1fs elapsed of %.1fs)",
            active_time_, desc_.min_burn_time);
    }
    if (active_time_ >= desc_.min_burn_time) Stop();
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
  active_time_    = 0.f;
  wreck_pending_  = false;

  LOG_F(INFO, "VehicleFireEffect: fire started at (%.1f, %.1f, %.1f)",
        world_pos.x, world_pos.y, world_pos.z);
}

void VehicleFireEffect::Stop() {
  if (!active_) return;
  active_->Stop();
  active_ = nullptr;
  active_time_   = 0.f;
  wreck_pending_ = false;

  LOG_F(INFO, "VehicleFireEffect: fire stopped");
}

bool VehicleFireEffect::AllZonesBelowThreshold() const {
  return std::none_of(kAllZones.begin(), kAllZones.end(), [this](game::DamageZone zone) {
    return damage_->GetDamageFraction(zone) >= desc_.damage_threshold;
  });
}

}  // namespace vfx
