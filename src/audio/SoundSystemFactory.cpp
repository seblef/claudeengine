#include "audio/SoundSystemFactory.h"

namespace audio {

std::unique_ptr<abstract::ISoundSystem> SoundSystemFactory::Create() {
    // TODO(#660): instantiate platform-specific backend.
    //   Linux / macOS — OpenALSoundSystem
    //   Windows       — XAudio2SoundSystem
    return nullptr;
}

}  // namespace audio
