#include "openal/ALSoundBuffer.h"

#include <AL/alc.h>

namespace openal {

namespace {

ALenum ToALFormat(abstract::AudioFormat format) {
    switch (format) {
        case abstract::AudioFormat::kMono8:    return AL_FORMAT_MONO8;
        case abstract::AudioFormat::kMono16:   return AL_FORMAT_MONO16;
        case abstract::AudioFormat::kStereo8:  return AL_FORMAT_STEREO8;
        case abstract::AudioFormat::kStereo16: return AL_FORMAT_STEREO16;
    }
    return AL_FORMAT_MONO16;
}

int BytesPerFrame(abstract::AudioFormat format) {
    switch (format) {
        case abstract::AudioFormat::kMono8:    return 1;
        case abstract::AudioFormat::kMono16:   return 2;
        case abstract::AudioFormat::kStereo8:  return 2;
        case abstract::AudioFormat::kStereo16: return 4;
    }
    return 2;
}

int NumChannels(abstract::AudioFormat format) {
    switch (format) {
        case abstract::AudioFormat::kMono8:
        case abstract::AudioFormat::kMono16:   return 1;
        case abstract::AudioFormat::kStereo8:
        case abstract::AudioFormat::kStereo16: return 2;
    }
    return 1;
}

}  // namespace

ALSoundBuffer::ALSoundBuffer(const void* data, int size_bytes,
                             abstract::AudioFormat format, int sample_rate)
    : num_channels_(NumChannels(format)), sample_rate_(sample_rate) {
    alGenBuffers(1, &buffer_id_);
    if (buffer_id_ == 0)
        return;

    const int bpf = BytesPerFrame(format);
    num_frames_ = (bpf > 0) ? (size_bytes / bpf) : 0;

    alBufferData(buffer_id_, ToALFormat(format), data,
                 static_cast<ALsizei>(size_bytes),
                 static_cast<ALsizei>(sample_rate));

    if (alGetError() != AL_NO_ERROR) {
        alDeleteBuffers(1, &buffer_id_);
        buffer_id_ = 0;
    }
}

ALSoundBuffer::~ALSoundBuffer() {
    if (buffer_id_ != 0)
        alDeleteBuffers(1, &buffer_id_);
}

}  // namespace openal
