#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Mat4f.h"

namespace terrain {

class TerrainData;

// Per-layer settings stored in the map file.
struct FoliageLayerDesc {
  // cppcheck-suppress unusedStructMember
  std::string name;
  // cppcheck-suppress unusedStructMember
  std::string mesh_path;
  // cppcheck-suppress unusedStructMember
  std::string texture_path;         // relative to data/textures/
  float       spacing_min         = 0.5f;
  float       spacing_max         = 1.5f;
  float       scale_min           = 0.8f;
  float       scale_max           = 1.2f;
  float       cull_distance       = 80.f;
  float       billboard_distance  = 40.f;
};

// One foliage layer: a single-channel density map painted by the editor plus
// scatter settings. The density map shares the terrain heightmap resolution.
//
// Values in the density map range from 0 (no foliage) to 255 (maximum
// density). GenerateInstances() scatters mesh instances using a jittered grid
// weighted by density values and returns their world-space TRS matrices.
//
// PaintBrush() / EraseBrush() modify the density map in world space.
// After any modification the layer is marked dirty; RebuildInstances() must
// be called (or IsDirty() checked) before re-uploading to the GPU.
class FoliageLayer {
 public:
  // Constructs an empty layer at terrain resolution. Density is all-zero.
  FoliageLayer(int map_width, int map_height, FoliageLayerDesc desc);

  // Scatters instances over `data` using the current density map and settings.
  // Replaces the cached instance list and clears the dirty flag.
  void RebuildInstances(const TerrainData& data);

  // Paints density additively at world position (wx, wz) within radius metres.
  // strength in [0, 1] controls how fast density saturates (255) per call.
  void PaintBrush(float wx, float wz, float radius, float strength,
                  const TerrainData& data);

  // Erases density at (wx, wz) within radius. strength in [0, 1].
  void EraseBrush(float wx, float wz, float radius, float strength,
                  const TerrainData& data);

  void MarkDirty() { dirty_ = true; }
  [[nodiscard]] bool IsDirty() const { return dirty_; }

  // True if the density map has been painted since the last ClearSaveDirty()
  // call, meaning the on-disk .r8 file is stale.
  [[nodiscard]] bool IsSaveDirty() const { return save_dirty_; }
  void ClearSaveDirty() const { save_dirty_ = false; }

  // ---- Accessors -------------------------------------------------------------

  [[nodiscard]] const FoliageLayerDesc&     GetDesc()       const { return desc_; }
  [[nodiscard]] FoliageLayerDesc&           GetDesc()             { return desc_; }
  [[nodiscard]] const std::vector<uint8_t>& GetDensityMap() const { return density_map_; }
  [[nodiscard]] std::vector<uint8_t>&       GetDensityMap()       { return density_map_; }
  [[nodiscard]] const std::vector<core::Mat4f>& GetInstances() const { return instances_; }
  [[nodiscard]] int GetMapWidth()  const { return map_width_;  }
  [[nodiscard]] int GetMapHeight() const { return map_height_; }

 private:
  void BrushImpl(float wx, float wz, float radius, float strength,
                 bool erase, const TerrainData& data);

  FoliageLayerDesc         desc_;
  int                      map_width_;
  int                      map_height_;
  std::vector<uint8_t>     density_map_;
  std::vector<core::Mat4f> instances_;
  bool                     dirty_            = true;
  mutable bool             save_dirty_       = false;
};

}  // namespace terrain
