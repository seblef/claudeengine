#include "audio/VirtualSoundInstance.h"

namespace audio {

VirtualSoundInstance::VirtualSoundInstance(Sound* sound,
                                           const core::Vec3f& position,
                                           bool loop, int priority, float gain)
    : sound_(sound),
      position_(position),
      gain_(gain),
      priority_(priority),
      loop_(loop),
      state_(VirtualSoundState::kPlaying),
      real_source_(nullptr) {
    sound_->AddRef();
}

void VirtualSoundInstance::Stop() {
    if (real_source_) real_source_->Stop();
    state_ = VirtualSoundState::kStopped;
}

void VirtualSoundInstance::SetPosition(const core::Vec3f& position) {
    position_ = position;
    if (real_source_) real_source_->SetPosition(position_);
}

void VirtualSoundInstance::SetGain(float gain) {
    gain_ = gain;
    if (real_source_) real_source_->SetGain(gain_ * sound_->GetDesc().volume);
}

}  // namespace audio
