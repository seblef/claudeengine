#pragma once

#include <AL/al.h>

#include "abstract/AudioFormat.h"
#include "abstract/ISoundBuffer.h"

namespace openal {

// ISoundBuffer backed by an OpenAL buffer object.
//
// Created via ALSoundSystem::CreateBuffer(). The AL buffer is allocated on
// construction and freed on destruction. Check IsValid() after construction.
class ALSoundBuffer : public abstract::ISoundBuffer {
 public:
  ALSoundBuffer(const void* data, int size_bytes,
                abstract::AudioFormat format, int sample_rate);
  ~ALSoundBuffer() override;

  ALSoundBuffer(const ALSoundBuffer&) = delete;
  ALSoundBuffer& operator=(const ALSoundBuffer&) = delete;

  [[nodiscard]] int GetNumFrames() const override { return num_frames_; }
  [[nodiscard]] int GetNumChannels() const override { return num_channels_; }
  [[nodiscard]] int GetSampleRate() const override { return sample_rate_; }

  // Returns the native AL buffer handle for use by ALSoundSource.
  [[nodiscard]] ALuint GetALBuffer() const { return buffer_id_; }

  // Returns false if the AL buffer could not be created or data upload failed.
  [[nodiscard]] bool IsValid() const { return buffer_id_ != 0; }

 private:
  // cppcheck-suppress unusedStructMember
  ALuint buffer_id_  = 0;
  // cppcheck-suppress unusedStructMember
  int num_frames_    = 0;
  // cppcheck-suppress unusedStructMember
  int num_channels_  = 0;
  // cppcheck-suppress unusedStructMember
  int sample_rate_   = 0;
};

}  // namespace openal
