#pragma once

#include <memory>

#include "abstract/ISoundSystem.h"

namespace audio {

// Selects and instantiates the audio backend appropriate for the current platform.
//
// Usage:
//   auto sound_system = audio::SoundSystemFactory::Create();
//   if (sound_system && sound_system->Initialize()) { ... }
//
// Platform routing (added as concrete backends are implemented):
//   Linux / macOS  — OpenAL Soft
//   Windows        — XAudio2
class SoundSystemFactory {
 public:
  // Returns a newly constructed backend for the current platform.
  // Returns nullptr when no backend is available or the platform is unsupported.
  [[nodiscard]] static std::unique_ptr<abstract::ISoundSystem> Create();
};

}  // namespace audio
