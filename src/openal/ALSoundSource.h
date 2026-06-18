#pragma once

#include <AL/al.h>

#include "abstract/ISoundSource.h"

namespace openal {

// ISoundSource backed by an OpenAL source object.
//
// Created via ALSoundSystem::CreateSource(). Check IsValid() after
// construction before use.
class ALSoundSource : public abstract::ISoundSource {
 public:
  ALSoundSource();
  ~ALSoundSource() override;

  ALSoundSource(const ALSoundSource&) = delete;
  ALSoundSource& operator=(const ALSoundSource&) = delete;

  void SetBuffer(abstract::ISoundBuffer* buffer) override;
  void SetPosition(const core::Vec3f& position) override;
  void SetVelocity(const core::Vec3f& velocity) override;
  void SetGain(float gain) override;
  void SetPitch(float pitch) override;
  void SetLoop(bool loop) override;
  void Play() override;
  void Pause() override;
  void Stop() override;
  [[nodiscard]] bool IsPlaying() const override;

  // Returns false if the AL source could not be created.
  [[nodiscard]] bool IsValid() const { return source_id_ != 0; }

 private:
  // cppcheck-suppress unusedStructMember
  ALuint source_id_ = 0;
};

}  // namespace openal
