#include "editor/SoundPreviewPlayer.h"

#include <loguru.hpp>

#include "audio/ResourceManager.h"
#include "audio/Sound.h"
#include "audio/SoundSystemFactory.h"
#include "core/Vec3f.h"

namespace editor {

SoundPreviewPlayer::SoundPreviewPlayer()
    : sound_system_(audio::SoundSystemFactory::Create()) {
  if (!sound_system_) {
    LOG_F(WARNING, "SoundPreviewPlayer: no audio backend available");
    return;
  }
  if (!sound_system_->Initialize()) {
    LOG_F(WARNING, "SoundPreviewPlayer: audio backend failed to initialise");
    sound_system_.reset();
    return;
  }
  resources_ = std::make_unique<audio::ResourceManager>(sound_system_.get());
  source_     = sound_system_->CreateSource();
  if (!source_) {
    LOG_F(WARNING, "SoundPreviewPlayer: could not create preview source");
    resources_.reset();
    sound_system_->Shutdown();
    sound_system_.reset();
    return;
  }
  // Relative mode: position is listener-relative → always at listener → no
  // distance attenuation, no panning.
  source_->SetRelative(true);
  source_->SetPosition({0.f, 0.f, 0.f});
  is_ready_ = true;
}

SoundPreviewPlayer::~SoundPreviewPlayer() {
  Stop();
  source_.reset();
  resources_.reset();
  if (sound_system_) sound_system_->Shutdown();
}

void SoundPreviewPlayer::Play(const std::string& sound_name, float gain) {
  if (!is_ready_) return;

  Stop();

  current_sound_ = resources_->LoadSound(sound_name);
  if (!current_sound_) {
    LOG_F(WARNING, "SoundPreviewPlayer: could not load '%s'",
          sound_name.c_str());
    return;
  }

  source_->SetBuffer(current_sound_->GetBuffer());
  source_->SetGain(gain * current_sound_->GetDesc().volume);
  source_->SetLoop(false);
  source_->Play();
}

void SoundPreviewPlayer::Stop() {
  if (!is_ready_) return;
  source_->Stop();
  source_->SetBuffer(nullptr);
  if (current_sound_) {
    current_sound_->Release();
    current_sound_ = nullptr;
  }
}

void SoundPreviewPlayer::Update() {
  if (!is_ready_) return;
  if (current_sound_ && !source_->IsPlaying()) Stop();
  if (sound_system_) sound_system_->Update(0.f);
}

}  // namespace editor
