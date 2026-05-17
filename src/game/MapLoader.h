#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "game/GameLightDesc.h"
#include "game/GameObject.h"

namespace abstract { class VideoDevice; }

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
  std::vector<std::unique_ptr<GameObject>> objects;
};

// Parses a .map.yaml file and returns instantiated GameObject subclasses.
//
// Objects are NOT added to GameSystem — the caller does that.
// Missing assets: LOG_F(WARNING, ...) + fallback (default material / unit-cube mesh).
// Returns empty MapData (name="", map_size=120, objects empty) on parse failure.
class MapLoader {
 public:
  static MapData Load(const std::filesystem::path& path,
                      abstract::VideoDevice* video);
};

}  // namespace game
