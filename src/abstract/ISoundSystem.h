#pragma once

#include <memory>

#include "abstract/AudioFormat.h"
#include "abstract/ISoundBuffer.h"
#include "abstract/ISoundSource.h"
#include "core/Mat4f.h"

namespace abstract {

// Abstract audio backend. Concrete subclasses implement platform-specific audio APIs
// (e.g. OpenAL on Linux/Mac, XAudio2 on Windows).
//
// Lifetime: call Initialize() before any other method. Buffers and sources created
// by this system must be destroyed before Shutdown() is called.
class ISoundSystem {
 public:
  virtual ~ISoundSystem() = default;

  // ---- Lifecycle -----------------------------------------------------------

  // Opens the audio device and prepares the backend for use.
  // Returns true on success. On failure the system remains in an unusable state.
  [[nodiscard]] virtual bool Initialize() = 0;

  // Releases all backend resources and closes the audio device.
  // All ISoundBuffer and ISoundSource objects created by this system must
  // have been destroyed before this call.
  virtual void Shutdown() = 0;

  // ---- Per-frame -----------------------------------------------------------

  // Advances the audio backend by dt seconds. Must be called once per game frame.
  // Processes streaming, position updates, and Doppler recalculation.
  virtual void Update(float dt) = 0;

  // ---- Listener transform --------------------------------------------------

  // Updates the listener (camera) world-space transform used for spatialization.
  // The position is taken from the translation column; the orientation is
  // derived from the rotation portion (forward = -Z column, up = +Y column).
  virtual void SetListenerTransform(const core::Mat4f& transform) = 0;

  // ---- Resource factories --------------------------------------------------

  // Uploads raw PCM data to the audio device and returns an opaque buffer handle.
  // data        — pointer to raw PCM bytes (must not be null)
  // size_bytes  — total byte count of the PCM payload
  // format      — sample layout (mono/stereo, 8-bit/16-bit)
  // sample_rate — number of PCM frames per second (e.g. 44100, 48000)
  // Returns nullptr if the backend rejects the data.
  [[nodiscard]] virtual std::unique_ptr<ISoundBuffer> CreateBuffer(
      const void* data, int size_bytes, AudioFormat format, int sample_rate) = 0;

  // Creates an idle sound source ready to receive a buffer and play.
  // The returned source is positioned at the origin with gain=1, pitch=1,
  // and loop=false by default.
  [[nodiscard]] virtual std::unique_ptr<ISoundSource> CreateSource() = 0;
};

}  // namespace abstract
