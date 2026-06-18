#include "audio/SoundSystemFactory.h"

#ifdef __linux__
#include "openal/ALSoundSystem.h"
#endif

namespace audio {

std::unique_ptr<abstract::ISoundSystem> SoundSystemFactory::Create() {
#ifdef __linux__
    return std::make_unique<openal::ALSoundSystem>();
#else
    // TODO(#661): add XAudio2SoundSystem for Windows.
    return nullptr;
#endif
}

}  // namespace audio
