#include "terrain/TerrainData.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace terrain {

TerrainData::TerrainData(const uint16_t* data, int width, int height,
                         float meters_per_texel, float min_height,
                         float max_height)
    : data_(data, data + static_cast<std::size_t>(width) * height),
      width_(width),
      height_(height),
      meters_per_texel_(meters_per_texel),
      min_height_(min_height),
      max_height_(max_height) {
  assert(width > 0 && height > 0);
  assert(meters_per_texel > 0.f);
  assert(min_height <= max_height);
}

float TerrainData::SampleToHeight(uint16_t sample) const {
  constexpr float kScale = 1.f / 65535.f;
  return min_height_ + static_cast<float>(sample) * kScale * (max_height_ - min_height_);
}

float TerrainData::HeightAt(int tx, int tz) const {
  tx = std::clamp(tx, 0, width_  - 1);
  tz = std::clamp(tz, 0, height_ - 1);
  return SampleToHeight(data_[static_cast<std::size_t>(tz) * width_ + tx]);
}

float TerrainData::GetHeight(float x, float z) const {
  // Convert world-space coords to texel space.
  const float fx = x / meters_per_texel_;
  const float fz = z / meters_per_texel_;

  const int tx = static_cast<int>(std::floor(fx));
  const int tz = static_cast<int>(std::floor(fz));

  const float u = fx - static_cast<float>(tx);
  const float v = fz - static_cast<float>(tz);

  // Bilinear interpolation over the four surrounding texels.
  const float h00 = HeightAt(tx,     tz);
  const float h10 = HeightAt(tx + 1, tz);
  const float h01 = HeightAt(tx,     tz + 1);
  const float h11 = HeightAt(tx + 1, tz + 1);

  return h00 * (1.f - u) * (1.f - v)
       + h10 * u          * (1.f - v)
       + h01 * (1.f - u) * v
       + h11 * u          * v;
}

core::Vec3f TerrainData::GetNormal(float x, float z) const {
  // Central differences; step one texel in each direction.
  const float step = meters_per_texel_;

  const float hL = GetHeight(x - step, z);
  const float hR = GetHeight(x + step, z);
  const float hD = GetHeight(x, z - step);
  const float hU = GetHeight(x, z + step);

  // Tangent vectors along X and Z axes, each spanning 2*step in world space.
  // Normal = tangent_x × tangent_z (right-hand rule, Y-up convention).
  const core::Vec3f n{hL - hR, 2.f * step, hD - hU};
  return n.Normalized();
}

uint16_t TerrainData::GetSample(int tx, int tz) const {
  tx = std::clamp(tx, 0, width_  - 1);
  tz = std::clamp(tz, 0, height_ - 1);
  return data_[static_cast<std::size_t>(tz) * width_ + tx];
}

void TerrainData::SetSample(int tx, int tz, uint16_t value) {
  data_[static_cast<std::size_t>(tz) * width_ + tx] = value;
}

void TerrainData::ReplaceHeightmap(const uint16_t* data, int width, int height) {
  width_  = width;
  height_ = height;
  data_.assign(data, data + static_cast<std::size_t>(width) * height);
}

uint16_t TerrainData::HeightToSample(float h) const {
  const float range = max_height_ - min_height_;
  if (range <= 0.f) return 0u;
  const float t = (h - min_height_) / range;
  const float clamped = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
  return static_cast<uint16_t>(std::lround(clamped * 65535.f));
}

}  // namespace terrain
