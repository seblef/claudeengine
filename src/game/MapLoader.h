#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "environment/EnvironmentDesc.h"
#include "game/GameLightDesc.h"
#include "game/GameObject.h"

namespace abstract { class VideoDevice; }
namespace audio {
class ResourceManager;
class SoundManager;
}  // namespace audio

namespace game {

// Aggregates all data parsed from a single .map.yaml file.
//
// objects are fully-instantiated GameMesh / GameLight / GameCamera instances.
// They are NOT added to GameSystem — the caller is responsible for that.
struct MapData {
  // cppcheck-suppress unusedStructMember
  std::string name;
  // cppcheck-suppress unusedStructMember
  float       map_size    = 120.f;   // full floor side length; half_size = map_size / 2
  // cppcheck-suppress unusedStructMember
  GameLightDesc global_light;
  // cppcheck-suppress unusedStructMember
  environment::EnvironmentDesc environment_desc;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<GameObject>> objects;
};

// Parses a .map.yaml file and returns instantiated GameObject subclasses.
//
// Objects are NOT added to GameSystem — the caller does that.
// Missing assets: LOG_F(WARNING, ...) + fallback (default material / unit-cube mesh).
// Returns empty MapData (name="", map_size=120, objects empty) on parse failure.
//
// sound_manager and resource_manager are optional. When non-null, sound_emitter
// objects in the YAML are instantiated as GameSoundEmitter. When null, any
// sound_emitter entries in the YAML are silently skipped.
class MapLoader {
 public:
  static MapData Load(const std::filesystem::path& path,
                      abstract::VideoDevice* video,
                      audio::SoundManager*    sound_manager    = nullptr,
                      audio::ResourceManager* resource_manager = nullptr);
};

}  // namespace game
