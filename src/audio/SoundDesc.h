#pragma once

#include <string>

#include "audio/RolloffType.h"

namespace audio {

// Descriptor holding all metadata parsed from a .sound.yaml file.
// Can also be filled programmatically to construct a Sound without a file.
struct SoundDesc {
  // Path to the raw audio file, relative to the engine data folder.
  // cppcheck-suppress unusedStructMember
  std::string file;

  // Whether the sound loops when played to completion.
  // cppcheck-suppress unusedStructMember
  bool loop = false;

  // Scheduling priority. Higher values are preferred when audio sources are
  // scarce and the backend must choose which sounds to drop.
  // cppcheck-suppress unusedStructMember
  int priority = 0;

  // Distance-attenuation model applied to this sound.
  // cppcheck-suppress unusedStructMember
  RolloffType rolloff = RolloffType::kInverse;

  // Reference distance at which the source gain equals the volume field.
  // cppcheck-suppress unusedStructMember
  float min_distance = 1.0f;

  // Distance at which gain reaches its minimum (clamped or falls off to 0).
  // cppcheck-suppress unusedStructMember
  float max_distance = 100.0f;

  // Default playback gain [0, 1]. 1.0 = full volume.
  // cppcheck-suppress unusedStructMember
  float volume = 1.0f;

  // Default pitch multiplier. 1.0 = original pitch; < 1 = lower, > 1 = higher.
  // cppcheck-suppress unusedStructMember
  float pitch = 1.0f;
};

}  // namespace audio
