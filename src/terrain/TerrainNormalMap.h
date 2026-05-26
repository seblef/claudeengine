#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace abstract {
class RawTexture;
class VideoDevice;
}  // namespace abstract

namespace terrain {

class TerrainData;

// Computes and manages a world-space normal map derived from a TerrainData
// heightmap.
//
// Normals are encoded as RGBA8:
//   stored = N * 0.5 + 0.5  (per XYZ channel, alpha is always 255)
//
// Dirty-tile partial uploads let sculpt-brush strokes re-upload only the
// affected region rather than the entire texture.
//
// Bind at texture slot 1 in the terrain vertex shader.
class TerrainNormalMap {
 public:
  TerrainNormalMap() = default;
  ~TerrainNormalMap() = default;

  TerrainNormalMap(const TerrainNormalMap&)            = delete;
  TerrainNormalMap& operator=(const TerrainNormalMap&) = delete;

  // Computes normals for the entire heightmap using central differences and
  // stores the result in the CPU-side pixel buffer. Retains a pointer to
  // data for use by UploadTile(); data must outlive this object.
  void Build(const TerrainData& data);

  // Creates the GPU texture and uploads the full normal map.
  // Must be called after Build().
  void Upload(abstract::VideoDevice* video);

  // Recomputes and re-uploads a rectangular region after a sculpt edit.
  //
  // The region is inflated by 1 texel on each side to account for the
  // central-difference stencil spanning adjacent texels.
  //
  // texel_x / texel_z — top-left corner of the dirty region (texel coords)
  // w / h             — width and height of the dirty region in texels
  void UploadTile(abstract::VideoDevice* video,
                  int texel_x, int texel_z, int w, int h);

  // Returns the GPU texture to bind at slot 1 of the terrain VS.
  // Returns nullptr if Upload() has not been called yet.
  [[nodiscard]] abstract::RawTexture* GetTexture() const {
    return texture_.get();
  }

 private:
  // Encodes and stores the RGBA8 normal for texel (tx, tz) into pixels_.
  void ComputeTexel(int tx, int tz);

  // Recomputes normals for all texels in [x0, x1) x [z0, z1).
  void ComputeRegion(int x0, int z0, int x1, int z1);

  const TerrainData*               data_    = nullptr;  // not owned
  // cppcheck-suppress unusedStructMember
  std::vector<uint8_t>             pixels_;             // RGBA8, row-major
  std::unique_ptr<abstract::RawTexture> texture_;
  int width_  = 0;
  int height_ = 0;
};

}  // namespace terrain
