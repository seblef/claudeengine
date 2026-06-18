#pragma once

#include <memory>
#include <string>

#include "abstract/ISoundBuffer.h"
#include "abstract/ISoundSystem.h"
#include "audio/SoundDesc.h"
#include "core/Resource.h"

namespace audio {

// Reference-counted resource that holds decoded PCM audio ready for playback.
//
// A Sound owns one ISoundBuffer (created by the active ISoundSystem) and
// the metadata parsed from its .sound.yaml file. Multiple sound sources can
// attach to the same Sound's buffer simultaneously via ISoundSource::SetBuffer.
//
// Keyed by name (the stem of the .sound.yaml filename, e.g. "explosion" for
// data/sounds/explosion.sound.yaml). File-backed instances should always be
// obtained via GetOrLoad() to avoid duplicate loads.
//
// Lifecycle mirrors GameMaterial / MeshTemplate:
//   Sound* s = Sound::GetOrLoad("explosion", sound_system);
//   // … attach s->GetBuffer() to a source …
//   s->Release();  // destroys when ref-count reaches zero
class Sound : public core::Resource<std::string, Sound> {
 public:
  // Loads sound metadata from data/sounds/{name}.sound.yaml, decodes the
  // referenced audio file, and uploads PCM to sound_system.
  Sound(const std::string& name, abstract::ISoundSystem* sound_system);

  // Constructs from a pre-populated descriptor. Decodes desc.file and uploads
  // PCM. Use for programmatic sound creation without a .sound.yaml file.
  Sound(const std::string& name, const SoundDesc& desc,
        abstract::ISoundSystem* sound_system);

  // Returns the existing instance (AddRef'd) or loads a new one from disk.
  [[nodiscard]] static Sound* GetOrLoad(const std::string& name,
                                        abstract::ISoundSystem* sound_system);

  // Returns the GPU-side buffer handle. Never null after successful init.
  [[nodiscard]] abstract::ISoundBuffer* GetBuffer() const;

  // Returns the playback metadata parsed from the .sound.yaml file.
  [[nodiscard]] const SoundDesc& GetDesc() const { return desc_; }

 private:
  void InitFromDesc(abstract::ISoundSystem* sound_system);

  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::ISoundBuffer> buffer_;
  // cppcheck-suppress unusedStructMember
  SoundDesc desc_;
};

}  // namespace audio
