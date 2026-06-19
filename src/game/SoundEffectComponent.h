#pragma once

#include <string>

#include "core/Vec3f.h"

namespace audio {
class ResourceManager;
class Sound;
class SoundManager;
}  // namespace audio

namespace game {

// Lightweight component that triggers one-shot positional sound effects.
//
// Embed in any game entity and call Trigger() with the current world position
// to fire a fire-and-forget sound (footstep, explosion, UI event). The
// SoundManager auto-releases the hardware source when playback ends.
//
// The component holds an AddRef'd Sound* for its own lifetime. If either
// manager pointer is null, Trigger() is a safe no-op.
class SoundEffectComponent {
 public:
  // Loads sound_name from resource_manager and holds an AddRef'd Sound*.
  // sound_manager and resource_manager may be null (component becomes silent).
  SoundEffectComponent(const std::string& sound_name,
                       audio::SoundManager* sound_manager,
                       audio::ResourceManager* resource_manager,
                       int priority = 0,
                       float gain = 1.0f);

  // Releases the Sound resource reference.
  ~SoundEffectComponent();

  SoundEffectComponent(const SoundEffectComponent&)            = delete;
  SoundEffectComponent& operator=(const SoundEffectComponent&) = delete;
  SoundEffectComponent(SoundEffectComponent&&)                 = delete;
  SoundEffectComponent& operator=(SoundEffectComponent&&)      = delete;

  // Fires the sound once at worldPos and releases the instance on completion.
  // Safe no-op if the sound failed to load or if either manager is null.
  void Trigger(const core::Vec3f& worldPos);

  // Returns true if the sound resource loaded successfully.
  [[nodiscard]] bool IsReady() const { return sound_ != nullptr; }

 private:
  // cppcheck-suppress unusedStructMember
  audio::SoundManager*    sound_manager_;
  // cppcheck-suppress unusedStructMember
  audio::ResourceManager* resource_manager_;
  // cppcheck-suppress unusedStructMember
  audio::Sound*           sound_ = nullptr;
  // cppcheck-suppress unusedStructMember
  int                     priority_;
  // cppcheck-suppress unusedStructMember
  float                   gain_;
};

}  // namespace game
