#include "vfx/VehicleScrapeEffect.h"

#include <memory>
#include <utility>

#include "vfx/VFXScrape.h"
#include "vfx/VFXSystem.h"

namespace vfx {

VehicleScrapeEffect::VehicleScrapeEffect(const physics::ScrapeDesc& desc,
                                         audio::SoundManager* sound_manager,
                                         audio::ResourceManager* resource_manager)
    : desc_(desc),
      sound_manager_(sound_manager),
      resource_manager_(resource_manager) {}

VehicleScrapeEffect::~VehicleScrapeEffect() {
  Stop();
}

void VehicleScrapeEffect::Update(float dt) {
  if (!active_) return;
  time_since_contact_ += dt;
  if (time_since_contact_ >= desc_.contact_grace_time) Stop();
}

void VehicleScrapeEffect::RegisterContact(const core::Vec3f& world_point,
                                          const core::Vec3f& world_normal,
                                          const core::Vec3f& relative_velocity) {
  if (relative_velocity.Length() < desc_.min_speed) {
    Stop();
    return;
  }

  time_since_contact_ = 0.f;

  if (!active_) {
    if (!VFXSystem::IsInstanced()) return;
    auto effect = std::make_unique<VFXScrape>(desc_, sound_manager_, resource_manager_);
    active_ = static_cast<VFXScrape*>(VFXSystem::Instance().Spawn(
        std::move(effect), world_point, relative_velocity.Normalized()));
  }

  active_->UpdateContact(world_point, world_normal, relative_velocity);
}

void VehicleScrapeEffect::Stop() {
  if (!active_) return;
  active_->Stop();
  active_ = nullptr;
}

}  // namespace vfx
