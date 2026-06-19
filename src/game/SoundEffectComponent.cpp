#include "game/SoundEffectComponent.h"

#include <string>

#include <loguru.hpp>

#include "audio/ResourceManager.h"
#include "audio/Sound.h"
#include "audio/SoundManager.h"
#include "core/Vec3f.h"

namespace game {

SoundEffectComponent::SoundEffectComponent(const std::string& sound_name,
                                           audio::SoundManager* sound_manager,
                                           audio::ResourceManager* resource_manager,
                                           int priority, float gain)
    : sound_manager_(sound_manager),
      resource_manager_(resource_manager),
      priority_(priority),
      gain_(gain) {
  if (resource_manager_ && !sound_name.empty()) {
    sound_ = resource_manager_->LoadSound(sound_name);
    if (!sound_) {
      LOG_F(WARNING, "SoundEffectComponent: sound '%s' failed to load",
            sound_name.c_str());
    }
  }
}

SoundEffectComponent::~SoundEffectComponent() {
  if (sound_) {
    sound_->Release();
  }
}

void SoundEffectComponent::Trigger(const core::Vec3f& worldPos) {
  if (!sound_manager_ || !sound_) return;
  sound_manager_->PlayOnce(sound_, worldPos, priority_, gain_);
}

}  // namespace game
