#include "openal/ALSoundSystem.h"

#include <loguru.hpp>

#include "openal/ALSoundBuffer.h"
#include "openal/ALSoundSource.h"

namespace openal {

ALSoundSystem::~ALSoundSystem() {
    if (context_ || device_)
        DoShutdown();
}

bool ALSoundSystem::Initialize() {
    device_ = alcOpenDevice(nullptr);
    if (!device_) {
        LOG_F(ERROR, "OpenAL: failed to open default audio device");
        return false;
    }

    context_ = alcCreateContext(device_, nullptr);
    if (!context_) {
        LOG_F(ERROR, "OpenAL: failed to create audio context");
        alcCloseDevice(device_);
        device_ = nullptr;
        return false;
    }

    if (!alcMakeContextCurrent(context_)) {
        LOG_F(ERROR, "OpenAL: failed to make context current");
        alcDestroyContext(context_);
        alcCloseDevice(device_);
        context_ = nullptr;
        device_  = nullptr;
        return false;
    }

    LOG_F(INFO, "OpenAL: initialized — device: %s",
          alcGetString(device_, ALC_DEVICE_SPECIFIER));
    return true;
}

void ALSoundSystem::Shutdown() {
    DoShutdown();
}

void ALSoundSystem::DoShutdown() {
    alcMakeContextCurrent(nullptr);
    if (context_) {
        alcDestroyContext(context_);
        context_ = nullptr;
    }
    if (device_) {
        alcCloseDevice(device_);
        device_ = nullptr;
    }
}

void ALSoundSystem::Update(float /*dt*/) {}

void ALSoundSystem::SetListenerTransform(const core::Mat4f& transform) {
    alListener3f(AL_POSITION,
                 transform(0, 3), transform(1, 3), transform(2, 3));

    // AL_ORIENTATION: [forward.x, forward.y, forward.z, up.x, up.y, up.z].
    // OpenGL convention: camera looks down -Z, so forward = -Z column.
    const float orientation[6] = {
        -transform(0, 2), -transform(1, 2), -transform(2, 2),
         transform(0, 1),  transform(1, 1),  transform(2, 1),
    };
    alListenerfv(AL_ORIENTATION, orientation);
}

std::unique_ptr<abstract::ISoundBuffer> ALSoundSystem::CreateBuffer(
        const void* data, int size_bytes,
        abstract::AudioFormat format, int sample_rate) {
    auto buffer = std::make_unique<ALSoundBuffer>(data, size_bytes, format, sample_rate);
    if (!buffer->IsValid()) {
        LOG_F(ERROR, "OpenAL: failed to create sound buffer");
        return nullptr;
    }
    return buffer;
}

std::unique_ptr<abstract::ISoundSource> ALSoundSystem::CreateSource() {
    auto source = std::make_unique<ALSoundSource>();
    if (!source->IsValid()) {
        LOG_F(ERROR, "OpenAL: failed to create sound source");
        return nullptr;
    }
    return source;
}

}  // namespace openal
