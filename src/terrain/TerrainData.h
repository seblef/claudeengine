#pragma once

#include <cstdint>
#include <vector>

#include "core/Vec3f.h"

namespace terrain {

// Stores a heightmap on the CPU and provides world-space height/normal queries.
//
// The heightmap is stored as 16-bit unsigned integers; the full [0, 65535]
// range maps linearly to [min_height_, max_height_] in world-space metres.
// GetRawData() exposes the raw buffer for GPU upload — no platform headers are
// required by this class.
class TerrainData {
 public:
  // Constructs a terrain from a raw 16-bit heightmap.
  //
  // data             — row-major heightmap, width * height elements
  // width / height   — dimensions in texels
  // meters_per_texel — world-space scale (metres per texel)
  // min_height       — world-space Y that maps to uint16 value 0
  // max_height       — world-space Y that maps to uint16 value 65535
  TerrainData(const uint16_t* data, int width, int height,
              float meters_per_texel, float min_height, float max_height);

  // ---- Dimension accessors -------------------------------------------------

  int   GetTexelWidth()  const { return width_; }
  int   GetTexelHeight() const { return height_; }

  // World-space extents in metres.
  float GetWorldWidth()  const { return static_cast<float>(width_)  * meters_per_texel_; }
  float GetWorldHeight() const { return static_cast<float>(height_) * meters_per_texel_; }

  // ---- World-space queries -------------------------------------------------

  // Returns the bilinearly interpolated height at world position (x, z).
  // Clamps to the heightmap boundary when out of range.
  float GetHeight(float x, float z) const;

  // Returns the world-space unit normal at (x, z) computed via central
  // differences on the heightmap grid.
  core::Vec3f GetNormal(float x, float z) const;

  // ---- GPU upload ---------------------------------------------------------

  // Returns a pointer to the raw 16-bit heightmap buffer (row-major).
  const uint16_t* GetRawData() const { return data_.data(); }

 private:
  // Converts a raw uint16 sample to a world-space height value.
  float SampleToHeight(uint16_t sample) const;

  // Returns the height at texel (tx, tz), clamped to valid range.
  float HeightAt(int tx, int tz) const;

  std::vector<uint16_t> data_;
  int   width_;
  int   height_;
  float meters_per_texel_;
  // cppcheck-suppress unusedStructMember
  float min_height_;
  // cppcheck-suppress unusedStructMember
  float max_height_;
};

}  // namespace terrain
