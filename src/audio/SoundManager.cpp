#include "audio/SoundManager.h"

#include <algorithm>

namespace audio {

SoundManager::SoundManager(abstract::ISoundSystem* sound_system, int pool_size)
    : sound_system_(sound_system), pool_size_(pool_size) {
    real_sources_.reserve(pool_size);
    free_sources_.reserve(pool_size);
    for (int i = 0; i < pool_size; ++i) {
        auto src = sound_system_->CreateSource();
        free_sources_.push_back(src.get());
        real_sources_.push_back(std::move(src));
    }
}

SoundManager::~SoundManager() {
    StopAll();
}

VirtualSoundInstance* SoundManager::PlaySound(Sound* sound,
                                              const core::Vec3f& position,
                                              bool loop, int priority,
                                              float gain) {
    if (!sound || !sound->IsInitialized()) return nullptr;
    // VirtualSoundInstance constructor is private; SoundManager is a friend.
    instances_.push_back(std::unique_ptr<VirtualSoundInstance>(
        new VirtualSoundInstance(sound, position, loop, priority, gain)));
    return instances_.back().get();
}

void SoundManager::SetListenerTransform(const core::Mat4f& transform) {
    // Row-major matrix: translation lives in column 3 — m(row, 3) = data[row*4+3].
    listener_pos_ = {transform(0, 3), transform(1, 3), transform(2, 3)};
    sound_system_->SetListenerTransform(transform);
}

void SoundManager::Update(float dt) {
    // Step 1: clean up finished / explicitly stopped instances.
    for (auto it = instances_.begin(); it != instances_.end(); ) {
        VirtualSoundInstance& inst = **it;

        const bool natural_end = inst.real_source_
                                 && !inst.loop_
                                 && !inst.real_source_->IsPlaying();
        if (inst.state_ == VirtualSoundState::kStopped || natural_end) {
            if (inst.real_source_) {
                inst.real_source_->Stop();
                free_sources_.push_back(inst.real_source_);
                inst.real_source_ = nullptr;
            }
            inst.sound_->Release();
            it = instances_.erase(it);
        } else {
            ++it;
        }
    }

    // Step 2: score surviving instances; cull those beyond max_distance.
    struct Candidate {
        VirtualSoundInstance* inst;
        float score;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(instances_.size());

    for (auto& ptr : instances_) {
        VirtualSoundInstance& inst = *ptr;
        const float max_d  = inst.sound_->GetDesc().max_distance;
        const float dist2  = inst.position_.SquaredDistance(listener_pos_);

        if (dist2 > max_d * max_d) {
            if (inst.real_source_) {
                inst.real_source_->Pause();
                free_sources_.push_back(inst.real_source_);
                inst.real_source_ = nullptr;
            }
            inst.state_ = VirtualSoundState::kSuspended;
            continue;
        }

        const float score =
            static_cast<float>(inst.priority_ + 1) / (dist2 + 1.0f);
        candidates.push_back({&inst, score});
    }

    // Step 3: rank by score descending.
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  return a.score > b.score;
              });

    // Step 4a: release hardware sources from instances beyond pool capacity.
    for (int i = pool_size_; i < static_cast<int>(candidates.size()); ++i) {
        VirtualSoundInstance* inst = candidates[i].inst;
        if (inst->real_source_) {
            inst->real_source_->Pause();
            free_sources_.push_back(inst->real_source_);
            inst->real_source_ = nullptr;
        }
        inst->state_ = VirtualSoundState::kSuspended;
    }

    // Step 4b: assign hardware sources to the top-N candidates.
    const int top = std::min(static_cast<int>(candidates.size()), pool_size_);
    for (int i = 0; i < top; ++i) {
        VirtualSoundInstance* inst = candidates[i].inst;

        if (!inst->real_source_) {
            if (free_sources_.empty()) continue;  // pool exhausted (guard)
            inst->real_source_ = free_sources_.back();
            free_sources_.pop_back();
            inst->real_source_->SetBuffer(inst->sound_->GetBuffer());
            inst->real_source_->SetPosition(inst->position_);
            inst->real_source_->SetGain(
                inst->gain_ * inst->sound_->GetDesc().volume);
            inst->real_source_->SetPitch(inst->sound_->GetDesc().pitch);
            inst->real_source_->SetLoop(inst->loop_);
            inst->real_source_->Play();
            inst->state_ = VirtualSoundState::kPlaying;
        } else {
            inst->real_source_->SetPosition(inst->position_);
            inst->real_source_->SetGain(
                inst->gain_ * inst->sound_->GetDesc().volume);
        }
    }

    // Step 5: advance the audio backend.
    sound_system_->Update(dt);
}

void SoundManager::StopAll() {
    for (auto& ptr : instances_) {
        VirtualSoundInstance& inst = *ptr;
        if (inst.real_source_) {
            inst.real_source_->Stop();
            free_sources_.push_back(inst.real_source_);
            inst.real_source_ = nullptr;
        }
        inst.sound_->Release();
    }
    instances_.clear();
}

}  // namespace audio
