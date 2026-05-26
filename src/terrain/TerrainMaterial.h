#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "terrain/TerrainMaterialLayer.h"

namespace abstract {
class RawTexture;
class Texture;
class VideoDevice;
}  // namespace abstract

namespace YAML {
class Emitter;
class Node;
}  // namespace YAML

namespace terrain {

// Maximum number of splatmap blend layers.
inline constexpr int kMaxTerrainLayers = 4;

// Visual appearance of terrain through a 4-layer splatmap blend.
//
// Each layer owns an albedo and a normal-map texture (reference-counted via the
// VideoDevice registry). Blend weights are stored as RGBA8 in a CPU pixel buffer
// whose channels correspond to layers 0–3 (R, G, B, A). A mirror GPU RawTexture
// is kept in sync via UploadSplatTile() for incremental paint updates.
//
// YAML format (terrain_material sub-node):
//   layers:
//     - albedo: textures/grass_albedo.png
//       normal: textures/grass_normal.png
//       tiling: 8.0
//   splatmap: terrain_splat.png
class TerrainMaterial {
 public:
  TerrainMaterial() = default;
  ~TerrainMaterial();

  TerrainMaterial(const TerrainMaterial&)            = delete;
  TerrainMaterial& operator=(const TerrainMaterial&) = delete;

  // Parses layers and splatmap path, loads all textures, and uploads the
  // splatmap to GPU.
  //
  // node         — YAML sub-node at the terrain_material level
  // video        — GPU backend used to create textures
  // width/height — terrain texel dimensions; used to initialise a default
  //                all-layer-0 splatmap when no splatmap file exists on disk
  void Load(const YAML::Node& node, abstract::VideoDevice* video,
            int width, int height);

  // Serialises layer paths, tiling factors, and the splatmap path to YAML.
  // Pixel data is NOT written to disk by this method.
  void Save(YAML::Emitter& out) const;

  // Sets the blend weight for one splatmap channel at the given texel.
  //
  // texel_x / texel_z — texel coordinates in [0, width) × [0, height)
  // layer             — channel index in [0, kMaxTerrainLayers)
  // weight            — blend weight in [0, 1]; clamped and encoded to uint8
  void SetSplatWeight(int texel_x, int texel_z, int layer, float weight);

  // Re-uploads a rectangular dirty region of the CPU splatmap to the GPU texture.
  //
  // x / z — top-left corner in texel coordinates
  // w / h — width and height of the region in texels
  void UploadSplatTile(abstract::VideoDevice* video, int x, int z, int w, int h);

  // ---- Accessors -----------------------------------------------------------

  [[nodiscard]] int                         GetLayerCount()     const { return layer_count_; }
  [[nodiscard]] const TerrainMaterialLayer& GetLayer(int i)     const { return layers_[i]; }
  [[nodiscard]] abstract::Texture*          GetAlbedo(int i)    const { return albedo_[i]; }
  [[nodiscard]] abstract::Texture*          GetNormal(int i)    const { return normal_[i]; }
  [[nodiscard]] abstract::RawTexture*       GetSplatmap()       const { return splatmap_tex_.get(); }
  [[nodiscard]] int                         GetSplatWidth()     const { return splat_width_; }
  [[nodiscard]] int                         GetSplatHeight()    const { return splat_height_; }

 private:
  // Creates (or re-creates) the GPU splatmap texture from the full CPU buffer.
  void UploadFullSplatmap(abstract::VideoDevice* video);

  std::array<TerrainMaterialLayer, kMaxTerrainLayers> layers_   = {};
  std::array<abstract::Texture*,   kMaxTerrainLayers> albedo_   = {};
  std::array<abstract::Texture*,   kMaxTerrainLayers> normal_   = {};

  // cppcheck-suppress unusedStructMember
  std::vector<uint8_t>                   splatmap_pixels_;
  std::unique_ptr<abstract::RawTexture>  splatmap_tex_;
  // cppcheck-suppress unusedStructMember
  std::string                            splatmap_path_;

  int layer_count_  = 0;
  int splat_width_  = 0;
  int splat_height_ = 0;
};

}  // namespace terrain
