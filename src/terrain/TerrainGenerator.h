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

struct RidgedParams {
  int   seed        = 0;
  float scale       = 300.f;   // world-space noise period in metres
  int   octaves     = 6;
  float lacunarity  = 2.0f;    // frequency multiplier per octave
  float gain        = 2.0f;    // controls sharpness of ridges
  float offset      = 1.0f;   // lifts the noise before inversion
};

struct ErosionParams {
  int   iterations  = 50000;  // number of simulated droplets
  int   seed        = 0;
  float inertia     = 0.05f;  // [0,1] — how much droplet keeps direction
  float capacity    = 4.0f;   // sediment capacity factor
  float deposition  = 0.3f;   // fraction of excess sediment deposited per step
  float erosion     = 0.3f;   // erosion rate
  float evaporation = 0.01f;  // water evaporation per step
  int   max_steps   = 64;     // max steps per droplet lifetime
  float min_slope   = 0.01f;  // minimum slope to avoid flat-area artefacts
  float gravity     = 4.0f;
};

// Generates procedural terrain heightmaps using various noise algorithms.
class TerrainGenerator {
 public:
  // Generates a width×height fBm heightmap. Output values span [0, 65535]
  // mapped linearly to [min_height, max_height].
  static std::vector<uint16_t> GenerateFbm(
      int width, int height,
      float meters_per_texel,
      float min_height, float max_height,
      const FbmParams& params);

  // Generates a width×height ridged multi-fractal heightmap. Output values
  // span [0, 65535] mapped linearly to [min_height, max_height].
  static std::vector<uint16_t> GenerateRidged(
      int width, int height,
      float meters_per_texel,
      float min_height, float max_height,
      const RidgedParams& params);

  // Applies particle-based hydraulic erosion to buf in-place. buf is a
  // row-major width×height uint16_t heightmap. All computation runs in
  // normalised [0,1] float space; conversion to/from uint16_t happens only
  // at the boundaries. Emits a loguru WARNING when texel count exceeds 1M.
  static void Erode(std::vector<uint16_t>& buf, int width, int height,
                    float meters_per_texel, const ErosionParams& params);
};

}  // namespace terrain
