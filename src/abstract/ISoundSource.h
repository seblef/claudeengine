#pragma once

#include "core/Vec3f.h"

namespace abstract {

class ISoundBuffer;

// A playing instance that drives a single ISoundBuffer through the audio pipeline.
//
// Created via ISoundSystem::CreateSource(). Call SetBuffer() to bind a buffer,
// then Play() to start playback. All spatial and amplitude parameters can be
// updated at any time, including while playing.
class ISoundSource {
 public:
  virtual ~ISoundSource() = default;

  // Binds the given buffer for playback. If nullptr, the source is silent.
  // The buffer must outlive the source while the source is active.
  virtual void SetBuffer(ISoundBuffer* buffer) = 0;

  // ---- Spatial parameters --------------------------------------------------

  // Sets the 3D world-space position used for distance attenuation and panning.
  virtual void SetPosition(const core::Vec3f& position) = 0;

  // Sets the 3D velocity in world space, used for Doppler pitch shift.
  virtual void SetVelocity(const core::Vec3f& velocity) = 0;

  // Distance at which the source plays at full gain. Attenuation begins
  // beyond this point. Mirrors AL_REFERENCE_DISTANCE / SoundDesc::min_distance.
  virtual void SetReferenceDistance(float distance) = 0;

  // Distance at which attenuation is clamped to its minimum. Beyond this
  // point the source is culled by SoundManager. Mirrors AL_MAX_DISTANCE /
  // SoundDesc::max_distance.
  virtual void SetMaxDistance(float distance) = 0;

  // ---- Amplitude / pitch ---------------------------------------------------

  // Sets the linear gain (volume) multiplier. 0.0 is silent, 1.0 is nominal.
  // Values above 1.0 are permitted but may clip depending on the backend.
  virtual void SetGain(float gain) = 0;

  // Sets the pitch multiplier. 1.0 is original pitch; 2.0 doubles frequency,
  // 0.5 halves it. Valid range is backend-dependent; OpenAL accepts [0.5, 2.0].
  virtual void SetPitch(float pitch) = 0;

  // When relative is true the source position is interpreted relative to the
  // listener rather than in world space. Setting position to (0,0,0) with
  // relative=true produces flat 2D audio with no distance attenuation or
  // stereo panning — useful for UI sounds and editor preview playback.
  virtual void SetRelative(bool relative) = 0;

  // ---- Playback control ----------------------------------------------------

  // Enables or disables looping. When true, the buffer restarts on exhaustion.
  virtual void SetLoop(bool loop) = 0;

  // Starts or resumes playback from the current position.
  virtual void Play() = 0;

  // Suspends playback; a subsequent Play() resumes from the same position.
  virtual void Pause() = 0;

  // Stops playback and rewinds to the start of the buffer.
  virtual void Stop() = 0;

  // Returns true if the source is currently playing (not paused or stopped).
  [[nodiscard]] virtual bool IsPlaying() const = 0;
};

}  // namespace abstract
