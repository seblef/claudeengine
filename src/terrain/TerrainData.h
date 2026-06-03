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

  int   GetTexelWidth()    const { return width_; }
  int   GetTexelHeight()   const { return height_; }
  float GetMetersPerTexel() const { return meters_per_texel_; }
  float GetMinHeight()     const { return min_height_; }
  float GetMaxHeight()     const { return max_height_; }

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

  // ---- Height range setters -----------------------------------------------

  // Updates the world-space height interpretation without modifying the raw
  // uint16_t samples. The renderer must be notified separately via
  // TerrainRenderer::SetHeightRange(); the normal map must be rebuilt via
  // TerrainNormalMap::Build().
  void SetMinHeight(float h) { min_height_ = h; UpdateRange(); }
  void SetMaxHeight(float h) { max_height_ = h; UpdateRange(); }

  // ---- Save-dirty tracking ------------------------------------------------

  // True if the raw heightmap samples have changed since the last ClearDirty()
  // call, meaning the on-disk .r16 file is stale.
  [[nodiscard]] bool IsDirty() const { return dirty_; }
  void ClearDirty() const { dirty_ = false; }

  // ---- Sculpt accessors ---------------------------------------------------

  // Returns the raw uint16_t sample at texel (tx, tz), clamped to valid range.
  [[nodiscard]] uint16_t GetSample(int tx, int tz) const;

  // Sets the raw uint16_t sample at texel (tx, tz).
  // Caller must ensure tx in [0, width_-1] and tz in [0, height_-1].
  void SetSample(int tx, int tz, uint16_t value);

  // Converts a world-space height to the nearest uint16_t sample, clamped to
  // [0, 65535].
  [[nodiscard]] uint16_t HeightToSample(float h) const;

  // Replaces the full heightmap buffer. Width and height may differ from the
  // current dimensions; callers must then rebuild the normal map, re-upload
  // the GPU heightmap and rebuild the CDLOD quadtree.
  void ReplaceHeightmap(const uint16_t* data, int width, int height);

 private:
  // Converts a raw uint16 sample to a world-space height value.
  float SampleToHeight(uint16_t sample) const;

  // Returns the height at texel (tx, tz), clamped to valid range.
  float HeightAt(int tx, int tz) const;

  // Update the range from min and max height
  void UpdateRange();

  std::vector<uint16_t> data_;
  int   width_;
  int   height_;
  float meters_per_texel_;
  float min_height_;
  float max_height_;
  // cppcheck-suppress unusedStructMember
  float range_;
  // cppcheck-suppress unusedStructMember
  mutable bool dirty_ = false;
};

}  // namespace terrain
