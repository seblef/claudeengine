#pragma once

#include <string>

namespace terrain {

// Visual descriptor for one layer of the terrain splatmap blend.
//
// Paths are relative to the data/textures/ directory (the same root used by
// abstract::VideoDevice::CreateTexture). The tiling factor controls how many
// UV repeats fit in one world-space metre; larger values produce a denser,
// smaller-looking texture.
struct TerrainMaterialLayer {
  // cppcheck-suppress unusedStructMember
  std::string albedo_path;   // path to albedo texture
  // cppcheck-suppress unusedStructMember
  std::string normal_path;   // path to normal map texture
  float       tiling = 1.f;  // UV tiling factor (larger = more repeats per metre)
};

}  // namespace terrain
