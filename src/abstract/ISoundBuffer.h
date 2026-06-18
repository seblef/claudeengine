#pragma once

namespace abstract {

// Opaque handle to decoded PCM audio data resident on the audio device.
//
// Created via ISoundSystem::CreateBuffer(). Attach to an ISoundSource with
// ISoundSource::SetBuffer() before calling Play(). The system owns the GPU/driver
// resource; the unique_ptr wrapper owns the C++ object.
class ISoundBuffer {
 public:
  virtual ~ISoundBuffer() = default;

  // Returns the number of PCM frames (samples per channel) in the buffer.
  [[nodiscard]] virtual int GetNumFrames() const = 0;

  // Returns the number of audio channels (1 = mono, 2 = stereo).
  [[nodiscard]] virtual int GetNumChannels() const = 0;

  // Returns the sampling rate in Hz (e.g. 44100, 48000).
  [[nodiscard]] virtual int GetSampleRate() const = 0;
};

}  // namespace abstract
