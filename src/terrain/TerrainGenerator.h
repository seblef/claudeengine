#pragma once

#include <cstdint>
#include <vector>

namespace terrain {

struct FbmParams {
  int   seed        = 0;
  float scale       = 300.f;   // world-space noise period in metres
  int   octaves     = 6;
  float persistence = 0.5f;    // amplitude falloff per octave
  float lacunarity  = 2.0f;    // frequency multiplier per octave
};

// Generates procedural terrain heightmaps using fractal Brownian Motion noise.
class TerrainGenerator {
 public:
  // Generates a width×height fBm heightmap. Output values span [0, 65535]
  // mapped linearly to [min_height, max_height].
  static std::vector<uint16_t> GenerateFbm(
      int width, int height,
      float meters_per_texel,
      float min_height, float max_height,
      const FbmParams& params);
};

}  // namespace terrain
