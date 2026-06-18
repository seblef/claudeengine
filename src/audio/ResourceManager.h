#pragma once

#include <string>

#include "abstract/ISoundSystem.h"
#include "audio/Sound.h"

namespace audio {

// Central manager for Sound resources.
//
// Wraps Sound::GetOrLoad() / Sound::Get() behind a lifecycle-aware API and
// holds the ISoundSystem* required for buffer creation and hot-reload.
//
// All returned Sound* pointers are AddRef'd; callers must call Release() when
// they are done with the pointer.
//
// Usage:
//   audio::ResourceManager mgr(sound_system);
//   Sound* s = mgr.LoadSound("explosion");
//   // … attach s->GetBuffer() to a source …
//   s->Release();
class ResourceManager {
 public:
  // sound_system must outlive this ResourceManager.
  explicit ResourceManager(abstract::ISoundSystem* sound_system);

  // Returns the existing Sound (AddRef'd), or nullptr if not currently loaded.
  [[nodiscard]] Sound* GetSound(const std::string& name);

  // Returns an AddRef'd Sound, loading from disk if not already cached.
  // Returns nullptr on load failure.
  [[nodiscard]] Sound* LoadSound(const std::string& name);

  // Hot-reload: re-parses the .sound.yaml and recreates ISoundBuffer in-place.
  // Existing Sound* pointers remain valid; callers must re-fetch GetBuffer()
  // after this call. No-op if the sound is not currently loaded.
  void Reload(const std::string& name);

 private:
  // cppcheck-suppress unusedStructMember
  abstract::ISoundSystem* sound_system_;
};

}  // namespace audio
