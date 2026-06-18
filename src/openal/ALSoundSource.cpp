#include "openal/ALSoundSource.h"

#include "openal/ALSoundBuffer.h"

namespace openal {

ALSoundSource::ALSoundSource() {
    alGenSources(1, &source_id_);
}

ALSoundSource::~ALSoundSource() {
    if (source_id_ != 0) {
        alSourceStop(source_id_);
        alDeleteSources(1, &source_id_);
    }
}

void ALSoundSource::SetBuffer(abstract::ISoundBuffer* buffer) {
    ALuint al_buffer = 0;
    if (buffer)
        al_buffer = static_cast<ALSoundBuffer*>(buffer)->GetALBuffer();
    alSourcei(source_id_, AL_BUFFER, static_cast<ALint>(al_buffer));
}

void ALSoundSource::SetPosition(const core::Vec3f& position) {
    alSource3f(source_id_, AL_POSITION, position.x, position.y, position.z);
}

void ALSoundSource::SetVelocity(const core::Vec3f& velocity) {
    alSource3f(source_id_, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}

void ALSoundSource::SetGain(float gain) {
    alSourcef(source_id_, AL_GAIN, gain);
}

void ALSoundSource::SetPitch(float pitch) {
    alSourcef(source_id_, AL_PITCH, pitch);
}

void ALSoundSource::SetLoop(bool loop) {
    alSourcei(source_id_, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
}

void ALSoundSource::Play() {
    alSourcePlay(source_id_);
}

void ALSoundSource::Pause() {
    alSourcePause(source_id_);
}

void ALSoundSource::Stop() {
    alSourceStop(source_id_);
}

bool ALSoundSource::IsPlaying() const {
    ALint state;
    alGetSourcei(source_id_, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

}  // namespace openal
