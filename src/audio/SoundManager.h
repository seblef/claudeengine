#pragma once

#include <list>
#include <memory>
#include <vector>

#include "abstract/ISoundSource.h"
#include "abstract/ISoundSystem.h"
#include "audio/Sound.h"
#include "audio/VirtualSoundInstance.h"
#include "core/Mat4f.h"
#include "core/Vec3f.h"

namespace audio {

// Multiplexes an unbounded list of virtual sound instances onto a fixed pool
// of hardware ISoundSource objects.
//
// Each Update() frame:
//   1. Cleans up instances that finished or were stopped.
//   2. Culls instances whose emitter lies beyond Sound::max_distance.
//   3. Ranks surviving instances by score = (priority+1) / (distance²+1).
//   4. Assigns hardware sources to the top-N; suspends (silences) the rest.
//   5. Forwards dt to the audio backend.
//
// The pool is pre-allocated at construction. pool_size controls how many
// sounds can be simultaneously audible; typical values are 16–64.
//
// Usage:
//   audio::SoundManager mgr(sound_system, 32);
//   mgr.SetListenerTransform(camera_matrix);
//   VirtualSoundInstance* h = mgr.PlaySound(sound, pos);
//   // each frame:
//   mgr.SetListenerTransform(camera_matrix);
//   mgr.Update(dt);
class SoundManager {
 public:
  // Pre-allocates pool_size hardware sources. sound_system must outlive this.
  explicit SoundManager(abstract::ISoundSystem* sound_system,
                        int pool_size = 32);

  ~SoundManager();

  SoundManager(const SoundManager&)            = delete;
  SoundManager& operator=(const SoundManager&) = delete;

  // Spawns a virtual sound at world-space position. The Sound's ref-count is
  // incremented and released when the instance ends. gain multiplies the
  // Sound's own volume field.
  // Returns a non-owning handle valid until Stop() is called on it or the
  // sound plays to its natural end; after either event the handle is
  // invalidated on the next Update(). Returns nullptr if sound is null or
  // not initialized.
  [[nodiscard]] VirtualSoundInstance* PlaySound(Sound* sound,
                                                const core::Vec3f& position,
                                                bool  loop     = false,
                                                int   priority = 0,
                                                float gain     = 1.0f);

  // Updates the listener world-space transform. Extracts listener position for
  // distance culling and forwards the full matrix to ISoundSystem.
  // Call once per frame, before Update().
  void SetListenerTransform(const core::Mat4f& transform);

  // Per-frame update: cull, rank, assign/release hardware sources, tick backend.
  void Update(float dt);

  // Stops all active instances and silences all hardware sources.
  void StopAll();

 private:
  // cppcheck-suppress unusedStructMember
  abstract::ISoundSystem* sound_system_;
  // cppcheck-suppress unusedStructMember
  int                     pool_size_;

  // Owns all hardware sources for their full lifetime.
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<abstract::ISoundSource>> real_sources_;
  // Raw pointers to sources not currently assigned to an instance.
  // cppcheck-suppress unusedStructMember
  std::vector<abstract::ISoundSource*> free_sources_;

  // Owns all live virtual instances. std::list guarantees pointer stability
  // so VirtualSoundInstance* handles remain valid across insertions/removals.
  // cppcheck-suppress unusedStructMember
  std::list<std::unique_ptr<VirtualSoundInstance>> instances_;

  // cppcheck-suppress unusedStructMember
  core::Vec3f listener_pos_;
};

}  // namespace audio
