#pragma once

#include "abstract/ISoundSource.h"
#include "audio/Sound.h"
#include "core/Vec3f.h"

namespace audio {

class SoundManager;

// Playback state of a VirtualSoundInstance.
enum class VirtualSoundState {
  kPlaying,    // Has a real hardware source and is audible.
  kSuspended,  // Awaiting a hardware source; will restart when one becomes free.
  kStopped,    // Playback ended or was stopped; removed on the next Update().
};

// Handle to a logical in-world sound event managed by SoundManager.
//
// SoundManager multiplexes VirtualSoundInstances onto a fixed pool of
// ISoundSource objects each frame based on priority and listener distance.
// Instances beyond max_distance or below the priority cut-off are suspended
// (silent, no real source) until conditions change.
//
// Obtain handles exclusively via SoundManager::PlaySound(). The handle is
// valid until Stop() is called OR the sound plays to its natural end (for
// non-looping sounds). After either event the handle is invalidated on the
// next SoundManager::Update() — do not use it after that point.
//
// Lifecycle:
//   VirtualSoundInstance* h = mgr.PlaySound(sound, pos, false, 0);
//   h->SetPosition(new_pos);  // update position each frame if emitter moves
//   h->Stop();                // stop early; handle invalid after next Update()
class VirtualSoundInstance {
 public:
  // Marks this instance as stopped. If a hardware source is assigned it is
  // silenced immediately. The handle must not be used after the next Update().
  void Stop();

  // Updates the emitter's world-space position. Immediately propagates to the
  // hardware source if one is currently assigned.
  void SetPosition(const core::Vec3f& position);

  // Sets the gain multiplier applied on top of Sound's base volume.
  // 0.0 = silent, 1.0 = nominal (Sound's own volume field).
  void SetGain(float gain);

  [[nodiscard]] const core::Vec3f& GetPosition() const { return position_; }
  [[nodiscard]] float              GetGain()      const { return gain_; }
  [[nodiscard]] int                GetPriority()  const { return priority_; }
  [[nodiscard]] VirtualSoundState  GetState()     const { return state_; }

  [[nodiscard]] bool IsPlaying()  const {
    return state_ == VirtualSoundState::kPlaying;
  }
  [[nodiscard]] bool IsStopped() const {
    return state_ == VirtualSoundState::kStopped;
  }

 private:
  friend class SoundManager;

  VirtualSoundInstance(Sound* sound, const core::Vec3f& position,
                       bool loop, int priority, float gain);

  // cppcheck-suppress unusedStructMember
  Sound*                  sound_;
  // cppcheck-suppress unusedStructMember
  core::Vec3f             position_;
  // cppcheck-suppress unusedStructMember
  float                   gain_;
  // cppcheck-suppress unusedStructMember
  int                     priority_;
  // cppcheck-suppress unusedStructMember
  bool                    loop_;
  // cppcheck-suppress unusedStructMember
  VirtualSoundState       state_;
  // cppcheck-suppress unusedStructMember
  abstract::ISoundSource* real_source_;
};

}  // namespace audio
