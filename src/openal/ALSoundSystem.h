#pragma once

#include <memory>

#include <AL/alc.h>

#include "abstract/ISoundSystem.h"

namespace openal {

// ISoundSystem backed by OpenAL Soft.
//
// Opens the default audio device on Initialize() and creates an AL context.
// All ISoundBuffer and ISoundSource objects must be destroyed before
// Shutdown() is called (or the destructor runs).
class ALSoundSystem : public abstract::ISoundSystem {
 public:
  ALSoundSystem() = default;
  ~ALSoundSystem() override;

  ALSoundSystem(const ALSoundSystem&) = delete;
  ALSoundSystem& operator=(const ALSoundSystem&) = delete;

  [[nodiscard]] bool Initialize() override;
  void Shutdown() override;

  // No-op: OpenAL Soft processes sources asynchronously in its own thread.
  void Update(float dt) override;

  // Extracts position (column 3) and orientation (forward = −Z col, up = +Y
  // col) from the world-space transform and forwards them to the AL listener.
  void SetListenerTransform(const core::Mat4f& transform) override;

  [[nodiscard]] std::unique_ptr<abstract::ISoundBuffer> CreateBuffer(
      const void* data, int size_bytes,
      abstract::AudioFormat format, int sample_rate) override;

  [[nodiscard]] std::unique_ptr<abstract::ISoundSource> CreateSource() override;

 private:
  // Non-virtual helper called by both ~ALSoundSystem() and Shutdown() to avoid
  // a virtual-call-in-destructor warning.
  void DoShutdown();

  // cppcheck-suppress unusedStructMember
  ALCdevice*  device_  = nullptr;
  // cppcheck-suppress unusedStructMember
  ALCcontext* context_ = nullptr;
};

}  // namespace openal
