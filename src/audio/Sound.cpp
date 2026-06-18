#include "audio/Sound.h"

#include <loguru.hpp>
#include <yaml-cpp/yaml.h>

#include "audio/AudioDecoderUtils.h"
#include "audio/RolloffType.h"
#include "core/Config.h"
#include "core/YamlUtils.h"

namespace audio {

namespace {

RolloffType ParseRolloff(const std::string& s) {
    if (s == "linear")      return RolloffType::kLinear;
    if (s == "exponential") return RolloffType::kExponential;
    return RolloffType::kInverse;
}

SoundDesc ParseDesc(const YAML::Node& root) {
    SoundDesc desc;
    if (root["file"])         desc.file         = root["file"].as<std::string>();
    if (root["loop"])         desc.loop         = root["loop"].as<bool>();
    if (root["priority"])     desc.priority     = root["priority"].as<int>();
    if (root["rolloff"])      desc.rolloff      = ParseRolloff(root["rolloff"].as<std::string>());
    if (root["min_distance"]) desc.min_distance = root["min_distance"].as<float>();
    if (root["max_distance"]) desc.max_distance = root["max_distance"].as<float>();
    if (root["volume"])       desc.volume       = root["volume"].as<float>();
    if (root["pitch"])        desc.pitch        = root["pitch"].as<float>();
    return desc;
}

}  // namespace

Sound::Sound(const std::string& name, abstract::ISoundSystem* sound_system)
    : Resource(name) {
    const auto yaml_path =
        core::Config::GetDataFolder() / "sounds" / (name + ".sound.yaml");
    try {
        const YAML::Node root = core::LoadYamlFile(yaml_path);
        desc_ = ParseDesc(root);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Sound '%s': YAML load error: %s", name.c_str(), e.what());
        return;
    }
    InitFromDesc(sound_system);
}

Sound::Sound(const std::string& name, const SoundDesc& desc,
             abstract::ISoundSystem* sound_system)
    : Resource(name), desc_(desc) {
    InitFromDesc(sound_system);
}

// static
Sound* Sound::GetOrLoad(const std::string& name,
                        abstract::ISoundSystem* sound_system) {
    Sound* existing = Get(name);
    if (existing) {
        existing->AddRef();
        return existing;
    }
    return new Sound(name, sound_system);
}

abstract::ISoundBuffer* Sound::GetBuffer() const {
    return buffer_.get();
}

void Sound::Reload(abstract::ISoundSystem* sound_system) {
    const auto yaml_path =
        core::Config::GetDataFolder() / "sounds" / (GetId() + ".sound.yaml");
    try {
        const YAML::Node root = core::LoadYamlFile(yaml_path);
        desc_ = ParseDesc(root);
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Sound '%s': reload YAML error: %s", GetId().c_str(), e.what());
        return;
    }
    buffer_.reset();
    initialized_ = false;
    InitFromDesc(sound_system);
}

void Sound::InitFromDesc(abstract::ISoundSystem* sound_system) {
    if (desc_.file.empty()) {
        LOG_F(ERROR, "Sound '%s': no audio file specified", GetId().c_str());
        return;
    }

    const auto audio_path = core::Config::GetDataFolder() / desc_.file;
    const DecodedAudio decoded = DecodeAudioFile(audio_path);

    if (decoded.channels == 0) {
        LOG_F(ERROR, "Sound '%s': decode failed for '%s'",
              GetId().c_str(), audio_path.c_str());
        return;
    }

    const abstract::AudioFormat format = AudioFormatFromChannels(decoded.channels);
    const int size_bytes =
        static_cast<int>(decoded.pcm.size()) * static_cast<int>(sizeof(int16_t));

    buffer_ = sound_system->CreateBuffer(
        decoded.pcm.data(), size_bytes, format, decoded.sample_rate);

    if (!buffer_) {
        LOG_F(ERROR, "Sound '%s': ISoundSystem::CreateBuffer failed",
              GetId().c_str());
        return;
    }

    initialized_ = true;
}

}  // namespace audio
