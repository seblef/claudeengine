#include "terrain/TerrainNormalMap.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

#include <loguru.hpp>

#include "abstract/RawTexture.h"
#include "abstract/VideoDevice.h"
#include "terrain/TerrainData.h"

namespace terrain {

namespace {

// Returns world-space height at texel centre (tx, tz), clamped to boundary.
float HeightAt(const TerrainData& data, int tx, int tz) {
  tx = std::clamp(tx, 0, data.GetTexelWidth()  - 1);
  tz = std::clamp(tz, 0, data.GetTexelHeight() - 1);
  const float mpt = data.GetMetersPerTexel();
  return data.GetHeight(static_cast<float>(tx) * mpt,
                        static_cast<float>(tz) * mpt);
}

uint8_t EncodeChannel(float v) {
  return static_cast<uint8_t>(std::lround((v * 0.5f + 0.5f) * 255.f));
}

}  // namespace

void TerrainNormalMap::ComputeTexel(int tx, int tz) {
  const float inv2 = 1.f / (2.f * data_->GetMetersPerTexel());
  const float dx =
      (HeightAt(*data_, tx + 1, tz) - HeightAt(*data_, tx - 1, tz)) * inv2;
  const float dz =
      (HeightAt(*data_, tx, tz + 1) - HeightAt(*data_, tx, tz - 1)) * inv2;

  const float nx = -dx;
  const float ny = 1.f;
  const float nz = -dz;
  const float inv_len =
      1.f / std::sqrt(nx * nx + ny * ny + nz * nz);

  uint8_t* dst =
      pixels_.data() + static_cast<std::size_t>(tz * width_ + tx) * 4;
  dst[0] = EncodeChannel(nx * inv_len);
  dst[1] = EncodeChannel(ny * inv_len);
  dst[2] = EncodeChannel(nz * inv_len);
  dst[3] = 255u;
}

void TerrainNormalMap::ComputeRegion(int x0, int z0, int x1, int z1) {
  for (int z = z0; z < z1; ++z)
    for (int x = x0; x < x1; ++x)
      ComputeTexel(x, z);
}

void TerrainNormalMap::Build(const TerrainData& data) {
  data_   = &data;
  width_  = data.GetTexelWidth();
  height_ = data.GetTexelHeight();
  pixels_.resize(static_cast<std::size_t>(width_) * height_ * 4);

  ComputeRegion(0, 0, width_, height_);

  LOG_F(INFO, "TerrainNormalMap::Build — %dx%d texels", width_, height_);
}

void TerrainNormalMap::Upload(abstract::VideoDevice* video) {
  assert(!pixels_.empty() && "Call Build() before Upload()");
  texture_ = video->CreateNormalMapTexture(width_, height_, pixels_.data());
}

void TerrainNormalMap::UploadTile(abstract::VideoDevice* video,
                                  int texel_x, int texel_z, int w, int h) {
  assert(data_    && "Call Build() before UploadTile()");
  assert(texture_ && "Call Upload() before UploadTile()");

  // Inflate by 1 on each side: central differences sample one texel outward.
  const int x0 = std::max(0,       texel_x - 1);
  const int z0 = std::max(0,       texel_z - 1);
  const int x1 = std::min(width_,  texel_x + w + 1);
  const int z1 = std::min(height_, texel_z + h + 1);

  // Recompute the inflated region into pixels_.
  ComputeRegion(x0, z0, x1, z1);

  // Build a contiguous sub-buffer for the GPU upload.
  const int tile_w = x1 - x0;
  const int tile_h = z1 - z0;
  std::vector<uint8_t> tile(static_cast<std::size_t>(tile_w) * tile_h * 4);
  for (int z = z0; z < z1; ++z) {
    const uint8_t* src =
        pixels_.data() + static_cast<std::size_t>(z * width_ + x0) * 4;
    uint8_t* dst =
        tile.data() + static_cast<std::size_t>((z - z0) * tile_w) * 4;
    std::copy(src, src + static_cast<std::size_t>(tile_w) * 4, dst);
  }

  texture_->UpdateRegion(x0, z0, tile_w, tile_h, tile.data());

  LOG_F(9, "TerrainNormalMap::UploadTile — [%d,%d)+[%d,%d] → inflated [%d,%d)+[%d,%d]",
        texel_x, texel_z, w, h, x0, z0, tile_w, tile_h);
}

}  // namespace terrain
