#include "terrain/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include <loguru.hpp>
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

void TerrainGenerator::Erode(std::vector<uint16_t>& buf, int width, int height,
                             float /*meters_per_texel*/,
                             const ErosionParams& params) {
  const std::size_t n = static_cast<std::size_t>(width) * height;

  if (n > 1024UL * 1024UL) {
    LOG_F(WARNING,
          "TerrainGenerator::Erode: %dx%d terrain (%.1fM texels) may take"
          " several seconds",
          width, height, static_cast<double>(n) / 1e6);
  }

  // Normalise to [0, 1] float for all erosion arithmetic.
  std::vector<float> map(n);
  for (std::size_t i = 0; i < n; ++i)
    map[i] = static_cast<float>(buf[i]) / 65535.f;

  std::mt19937 rng(static_cast<uint32_t>(params.seed));
  std::uniform_real_distribution<float> dist_x(
      0.f, static_cast<float>(width  - 1));
  std::uniform_real_distribution<float> dist_z(
      0.f, static_cast<float>(height - 1));

  for (int iter = 0; iter < params.iterations; ++iter) {
    float px  = dist_x(rng);
    float pz  = dist_z(rng);
    float dx  = 0.f;
    float dz  = 0.f;
    float speed    = 1.f;
    float water    = 1.f;
    float sediment = 0.f;

    for (int step = 0; step < params.max_steps; ++step) {
      const int ix = static_cast<int>(px);
      const int iz = static_cast<int>(pz);

      if (ix < 0 || iz < 0 || ix >= width - 1 || iz >= height - 1) break;

      const float fx = px - static_cast<float>(ix);
      const float fz = pz - static_cast<float>(iz);

      // Four-corner heights of the cell the droplet is in.
      const float h00 = map[ iz      * width + ix    ];
      const float h10 = map[ iz      * width + ix + 1];
      const float h01 = map[(iz + 1) * width + ix    ];
      const float h11 = map[(iz + 1) * width + ix + 1];

      // Gradient within the bilinear cell.
      const float gx = (h10 - h00) * (1.f - fz) + (h11 - h01) * fz;
      const float gz = (h01 - h00) * (1.f - fx) + (h11 - h10) * fx;

      const float h_cur = h00 * (1.f - fx) * (1.f - fz) +
                          h10 *        fx  * (1.f - fz) +
                          h01 * (1.f - fx) *        fz  +
                          h11 *        fx  *        fz;

      // Update and normalise direction.
      dx = dx * params.inertia - gx * (1.f - params.inertia);
      dz = dz * params.inertia - gz * (1.f - params.inertia);
      const float len = std::sqrt(dx * dx + dz * dz);
      if (len < 1e-6f) break;
      dx /= len;
      dz /= len;

      const float new_px = px + dx;
      const float new_pz = pz + dz;

      if (new_px < 0.f || new_pz < 0.f ||
          new_px >= static_cast<float>(width  - 1) ||
          new_pz >= static_cast<float>(height - 1)) break;

      const int   nix = static_cast<int>(new_px);
      const int   niz = static_cast<int>(new_pz);
      const float nfx = new_px - static_cast<float>(nix);
      const float nfz = new_pz - static_cast<float>(niz);

      const float nh00 = map[ niz      * width + nix    ];
      const float nh10 = map[ niz      * width + nix + 1];
      const float nh01 = map[(niz + 1) * width + nix    ];
      const float nh11 = map[(niz + 1) * width + nix + 1];

      const float h_new = nh00 * (1.f - nfx) * (1.f - nfz) +
                          nh10 *        nfx  * (1.f - nfz) +
                          nh01 * (1.f - nfx) *        nfz  +
                          nh11 *        nfx  *        nfz;

      const float dh   = h_new - h_cur;
      const float slope = std::max(params.min_slope, -dh);
      const float cap   = slope * speed * water * params.capacity;

      if (sediment > cap || dh > 0.f) {
        // Deposit excess sediment or fill uphill step.
        const float deposit = (dh > 0.f)
            ? std::min(dh, sediment)
            : (sediment - cap) * params.deposition;
        sediment -= deposit;

        map[ iz      * width + ix    ] += deposit * (1.f - fx) * (1.f - fz);
        map[ iz      * width + ix + 1] += deposit *        fx  * (1.f - fz);
        map[(iz + 1) * width + ix    ] += deposit * (1.f - fx) *        fz;
        map[(iz + 1) * width + ix + 1] += deposit *        fx  *        fz;
      } else {
        // Erode material from the current cell.
        const float erode = std::min((cap - sediment) * params.erosion, -dh);
        sediment += erode;

        map[ iz      * width + ix    ] -= erode * (1.f - fx) * (1.f - fz);
        map[ iz      * width + ix + 1] -= erode *        fx  * (1.f - fz);
        map[(iz + 1) * width + ix    ] -= erode * (1.f - fx) *        fz;
        map[(iz + 1) * width + ix + 1] -= erode *        fx  *        fz;
      }

      speed = std::sqrt(std::max(0.f, speed * speed + dh * params.gravity));
      water *= (1.f - params.evaporation);

      px = new_px;
      pz = new_pz;
    }
  }

  // Clamp to [0, 1] and write back as uint16_t.
  for (std::size_t i = 0; i < n; ++i) {
    const float v = std::clamp(map[i], 0.f, 1.f);
    buf[i] = static_cast<uint16_t>(v * 65535.f + 0.5f);
  }
}

}  // namespace terrain
