#include "terrain/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <stb_perlin.h>

namespace terrain {

namespace {
// Large prime-based offsets used to derive per-seed domain shifts so that
// different integer seeds produce visibly different noise fields even though
// stb_perlin_noise3_seed truncates the seed to a single byte.
constexpr float kSeedScaleX = 127.1f;
constexpr float kSeedScaleZ = 311.7f;
}  // namespace

std::vector<uint16_t> TerrainGenerator::GenerateFbm(
    int width, int height,
    float meters_per_texel,
    float min_height, float max_height,
    const FbmParams& params) {
  const std::size_t n = static_cast<std::size_t>(width) * height;
  std::vector<float> raw(n);

  // Domain shift derived from seed so distinct integer seeds produce
  // different noise fields (stb seed wraps to uint8, so this is necessary).
  const float ox = static_cast<float>(params.seed) * kSeedScaleX;
  const float oz = static_cast<float>(params.seed) * kSeedScaleZ;

  const float inv_scale = 1.f / std::max(params.scale, 1e-6f);

  for (int z = 0; z < height; ++z) {
    for (int x = 0; x < width; ++x) {
      const float wx = (static_cast<float>(x) + 0.5f) * meters_per_texel;
      const float wz = (static_cast<float>(z) + 0.5f) * meters_per_texel;

      float nx = wx * inv_scale + ox;
      float nz = wz * inv_scale + oz;

      float amplitude  = 1.f;
      float frequency  = 1.f;
      float sum        = 0.f;

      const int oct = std::max(1, params.octaves);
      for (int o = 0; o < oct; ++o) {
        const int octave_seed = (params.seed + o * 73) & 0xFF;
        sum += amplitude * stb_perlin_noise3_seed(
            nx * frequency, 0.f, nz * frequency,
            0, 0, 0, octave_seed);
        amplitude *= params.persistence;
        frequency *= params.lacunarity;
      }

      raw[static_cast<std::size_t>(z * width + x)] = sum;
    }
  }

  // Normalise the full noise range to [0, 65535].
  float min_val = raw[0];
  float max_val = raw[0];
  for (std::size_t i = 1; i < n; ++i) {
    if (raw[i] < min_val) min_val = raw[i];
    if (raw[i] > max_val) max_val = raw[i];
  }

  const float range = (max_val > min_val) ? (max_val - min_val) : 1.f;

  std::vector<uint16_t> result(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float t = (raw[i] - min_val) / range;
    result[i] = static_cast<uint16_t>(t * 65535.f + 0.5f);
  }

  (void)min_height;
  (void)max_height;

  return result;
}

std::vector<uint16_t> TerrainGenerator::GenerateRidged(
    int width, int height,
    float meters_per_texel,
    float min_height, float max_height,
    const RidgedParams& params) {
  const std::size_t n = static_cast<std::size_t>(width) * height;
  std::vector<float> raw(n);

  const float ox = static_cast<float>(params.seed) * kSeedScaleX;
  const float oz = static_cast<float>(params.seed) * kSeedScaleZ;

  const float inv_scale = 1.f / std::max(params.scale, 1e-6f);
  const int   oct       = std::max(1, params.octaves);

  for (int z = 0; z < height; ++z) {
    for (int x = 0; x < width; ++x) {
      const float wx = (static_cast<float>(x) + 0.5f) * meters_per_texel;
      const float wz = (static_cast<float>(z) + 0.5f) * meters_per_texel;

      const float nx = wx * inv_scale + ox;
      const float nz = wz * inv_scale + oz;

      raw[static_cast<std::size_t>(z * width + x)] =
          stb_perlin_ridge_noise3(nx, 0.f, nz,
                                  params.lacunarity, params.gain,
                                  params.offset, oct);
    }
  }

  float min_val = raw[0];
  float max_val = raw[0];
  for (std::size_t i = 1; i < n; ++i) {
    if (raw[i] < min_val) min_val = raw[i];
    if (raw[i] > max_val) max_val = raw[i];
  }

  const float range = (max_val > min_val) ? (max_val - min_val) : 1.f;

  std::vector<uint16_t> result(n);
  for (std::size_t i = 0; i < n; ++i) {
    const float t = (raw[i] - min_val) / range;
    result[i] = static_cast<uint16_t>(t * 65535.f + 0.5f);
  }

  (void)min_height;
  (void)max_height;

  return result;
}

}  // namespace terrain
