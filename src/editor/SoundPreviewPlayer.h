#pragma once

#include <memory>
#include <string>

#include "abstract/ISoundSource.h"
#include "abstract/ISoundSystem.h"

namespace audio {
class ResourceManager;
class Sound;
}  // namespace audio

namespace editor {

// Lightweight audio player for flat 2D one-shot preview in the editor.
//
// Initialises its own ISoundSystem so the editor does not need a game-level
// SoundManager. Playback is source-relative (AL_SOURCE_RELATIVE) at position
// (0,0,0) — no listener position, no distance attenuation.
//
// Call IsReady() after construction before using Play(). Update() must be
// called once per editor frame to release finished sources.
class SoundPreviewPlayer {
 public:
  SoundPreviewPlayer();
  ~SoundPreviewPlayer();

  SoundPreviewPlayer(const SoundPreviewPlayer&)            = delete;
  SoundPreviewPlayer& operator=(const SoundPreviewPlayer&) = delete;

  // Returns true if the audio backend initialised successfully.
  [[nodiscard]] bool IsReady() const { return is_ready_; }

  // Returns the underlying sound system for sharing with other audio consumers.
  // Nullptr when the backend failed to initialise.
  [[nodiscard]] abstract::ISoundSystem* GetSoundSystem() const {
    return sound_system_.get();
  }

  // Plays sound_name at gain (0–1 range typical). Stops any in-progress
  // preview first. No-op if not ready or the sound cannot be loaded.
  void Play(const std::string& sound_name, float gain);

  // Stops the current preview immediately.
  void Stop();

  // Must be called once per frame: releases the source when playback ends.
  void Update();

 private:
  bool                                      is_ready_       = false;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ISoundSystem>   sound_system_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<audio::ResourceManager>   resources_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ISoundSource>   source_;
  // cppcheck-suppress unusedStructMember
  audio::Sound*                             current_sound_  = nullptr;
};

}  // namespace editor
