#include "terrain/TerrainMaterial.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <vector>

#include <loguru.hpp>
#include <yaml-cpp/yaml.h>

#include "abstract/RawTexture.h"
#include "abstract/Texture.h"
#include "abstract/VideoDevice.h"
#include "core/Config.h"

namespace terrain {

TerrainMaterial::~TerrainMaterial() {
  for (int i = 0; i < kMaxTerrainLayers; ++i) {
    if (albedo_[i]) albedo_[i]->Release();
    if (normal_[i]) normal_[i]->Release();
  }
}

void TerrainMaterial::Load(const YAML::Node& node,
                           abstract::VideoDevice* video,
                           int width, int height) {
  layer_count_ = 0;

  if (node["layers"]) {
    for (const auto& ln : node["layers"]) {
      if (layer_count_ >= kMaxTerrainLayers) break;
      const int i = layer_count_++;

      layers_[i].albedo_path = ln["albedo"] ? ln["albedo"].as<std::string>() : "";
      layers_[i].normal_path = ln["normal"] ? ln["normal"].as<std::string>() : "";
      layers_[i].tiling      = ln["tiling"] ? ln["tiling"].as<float>()       : 1.f;

      if (!layers_[i].albedo_path.empty()) {
        albedo_[i] = video->CreateTexture(layers_[i].albedo_path);
        if (!albedo_[i])
          LOG_F(WARNING, "TerrainMaterial: layer %d albedo '%s' not found",
                i, layers_[i].albedo_path.c_str());
      }
      if (!layers_[i].normal_path.empty()) {
        normal_[i] = video->CreateTexture(layers_[i].normal_path);
        if (!normal_[i])
          LOG_F(WARNING, "TerrainMaterial: layer %d normal '%s' not found",
                i, layers_[i].normal_path.c_str());
      }
    }
  }

  splatmap_path_ = node["splatmap"] ? node["splatmap"].as<std::string>() : "";

  // Attempt to load an existing splatmap PNG from disk.
  bool loaded = false;
  if (!splatmap_path_.empty()) {
    const auto path =
        core::Config::GetDataFolder() / "textures" / splatmap_path_;
    int sw = 0, sh = 0;
    loaded = video->LoadRGBA8File(path, &sw, &sh, splatmap_pixels_);
    if (loaded) {
      splat_width_  = sw;
      splat_height_ = sh;
      LOG_F(INFO, "TerrainMaterial: loaded splatmap '%s' (%dx%d)",
            splatmap_path_.c_str(), sw, sh);
    }
  }

  // Default splatmap: full weight on layer 0 (R=255, G=B=A=0).
  if (!loaded) {
    splat_width_  = width;
    splat_height_ = height;
    splatmap_pixels_.assign(
        static_cast<std::size_t>(width) * height * 4, 0u);
    for (std::size_t p = 0;
         p < static_cast<std::size_t>(width) * height; ++p) {
      splatmap_pixels_[p * 4] = 255u;
    }
    LOG_F(INFO,
          "TerrainMaterial: no splatmap file, initialised default %dx%d (layer 0)",
          width, height);
  }

  UploadFullSplatmap(video);
}

void TerrainMaterial::Save(YAML::Emitter& out) const {
  out << YAML::Key << "terrain_material" << YAML::Value << YAML::BeginMap;

  out << YAML::Key << "layers" << YAML::Value << YAML::BeginSeq;
  for (int i = 0; i < layer_count_; ++i) {
    out << YAML::BeginMap;
    out << YAML::Key << "albedo" << YAML::Value << layers_[i].albedo_path;
    out << YAML::Key << "normal" << YAML::Value << layers_[i].normal_path;
    out << YAML::Key << "tiling" << YAML::Value << layers_[i].tiling;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;

  if (!splatmap_path_.empty())
    out << YAML::Key << "splatmap" << YAML::Value << splatmap_path_;

  out << YAML::EndMap;
}

void TerrainMaterial::SetSplatWeight(int texel_x, int texel_z,
                                     int layer, float weight) {
  assert(texel_x >= 0 && texel_x < splat_width_);
  assert(texel_z >= 0 && texel_z < splat_height_);
  assert(layer   >= 0 && layer   < kMaxTerrainLayers);

  const std::size_t idx =
      (static_cast<std::size_t>(texel_z) * splat_width_ + texel_x) * 4 +
      static_cast<std::size_t>(layer);
  splatmap_pixels_[idx] =
      static_cast<uint8_t>(
          std::lround(std::clamp(weight, 0.f, 1.f) * 255.f));
}

void TerrainMaterial::UploadSplatTile(abstract::VideoDevice* /*video*/,
                                      int x, int z, int w, int h) {
  assert(splatmap_tex_ && "Call Load() before UploadSplatTile()");

  const int x0 = std::clamp(x,     0, splat_width_);
  const int z0 = std::clamp(z,     0, splat_height_);
  const int x1 = std::clamp(x + w, 0, splat_width_);
  const int z1 = std::clamp(z + h, 0, splat_height_);

  const int tile_w = x1 - x0;
  const int tile_h = z1 - z0;
  if (tile_w <= 0 || tile_h <= 0) return;

  std::vector<uint8_t> tile(
      static_cast<std::size_t>(tile_w) * tile_h * 4);
  for (int row = z0; row < z1; ++row) {
    const uint8_t* src =
        splatmap_pixels_.data() +
        static_cast<std::size_t>(row * splat_width_ + x0) * 4;
    uint8_t* dst =
        tile.data() +
        static_cast<std::size_t>((row - z0) * tile_w) * 4;
    std::copy(src, src + static_cast<std::size_t>(tile_w) * 4, dst);
  }

  splatmap_tex_->UpdateRegion(x0, z0, tile_w, tile_h, tile.data());

  LOG_F(9, "TerrainMaterial::UploadSplatTile — [%d,%d)+[%d,%d]",
        x0, z0, tile_w, tile_h);
}

void TerrainMaterial::UploadFullSplatmap(abstract::VideoDevice* video) {
  splatmap_tex_ = video->CreateNormalMapTexture(
      splat_width_, splat_height_, splatmap_pixels_.data());
}

bool TerrainMaterial::AddLayer() {
  if (layer_count_ >= kMaxTerrainLayers) return false;
  const int i = layer_count_++;
  layers_[i] = {"", "", 1.f};
  albedo_[i]  = nullptr;
  normal_[i]  = nullptr;
  return true;
}

void TerrainMaterial::SetLayerAlbedo(int i, const std::string& path,
                                     abstract::VideoDevice* video) {
  assert(i >= 0 && i < layer_count_);
  if (albedo_[i]) {
    albedo_[i]->Release();
    albedo_[i] = nullptr;
  }
  layers_[i].albedo_path = path;
  if (!path.empty()) {
    albedo_[i] = video->CreateTexture(path);
    if (!albedo_[i])
      LOG_F(WARNING, "TerrainMaterial: layer %d albedo '%s' not found",
            i, path.c_str());
  }
}

void TerrainMaterial::SetLayerNormal(int i, const std::string& path,
                                     abstract::VideoDevice* video) {
  assert(i >= 0 && i < layer_count_);
  if (normal_[i]) {
    normal_[i]->Release();
    normal_[i] = nullptr;
  }
  layers_[i].normal_path = path;
  if (!path.empty()) {
    normal_[i] = video->CreateTexture(path);
    if (!normal_[i])
      LOG_F(WARNING, "TerrainMaterial: layer %d normal '%s' not found",
            i, path.c_str());
  }
}

void TerrainMaterial::SetLayerTiling(int i, float tiling) {
  assert(i >= 0 && i < layer_count_);
  layers_[i].tiling = tiling;
}

}  // namespace terrain
