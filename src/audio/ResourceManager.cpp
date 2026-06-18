#include "audio/ResourceManager.h"

namespace audio {

ResourceManager::ResourceManager(abstract::ISoundSystem* sound_system)
    : sound_system_(sound_system) {}

// cppcheck-suppress functionStatic
Sound* ResourceManager::GetSound(const std::string& name) {
    Sound* s = Sound::Get(name);
    if (s) s->AddRef();
    return s;
}

Sound* ResourceManager::LoadSound(const std::string& name) {
    return Sound::GetOrLoad(name, sound_system_);
}

void ResourceManager::Reload(const std::string& name) {
    Sound* s = Sound::Get(name);
    if (s) s->Reload(sound_system_);
}

}  // namespace audio
