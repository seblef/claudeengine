#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace environment {

// Per-layer placement and rendering parameters stored in the map file.
//
// Mirrors FoliageLayerDesc but carries tree-specific parameters (LOD distances,
// wind sway strengths) and a billboard path in addition to the full-res mesh.
struct TreeLayerDesc {
  // cppcheck-suppress unusedStructMember
  std::string mesh_path;             // relative to data/  (e.g. meshes/oak.obj)
  // cppcheck-suppress unusedStructMember
  std::string billboard_path;        // relative to data/textures/
  float spacing_min          = 3.f;   // minimum distance between instances (m)
  float spacing_max          = 8.f;   // cell size for jitter placement (m)
  float scale_min            = 0.8f;
  float scale_max            = 1.3f;
  float billboard_distance   = 60.f;  // switch to billboard beyond this distance (m)
  float cull_distance        = 120.f;
  float trunk_sway_strength  = 0.04f;
  float leaf_sway_strength   = 0.08f;
};

// One tree layer: a density map (same format as FoliageLayer) plus per-layer
// placement and LOD settings.
//
// density_map values range from 0 (no trees) to 255 (maximum density).
// Instance placement is done by TreeRenderer::Build(); this class only stores
// the CPU data.
class TreeLayer {
 public:
  TreeLayer(int map_width, int map_height, TreeLayerDesc desc);

  void MarkDirty() { dirty_ = true; }
  [[nodiscard]] bool IsDirty() const { return dirty_; }

  [[nodiscard]] const TreeLayerDesc&     GetDesc()       const { return desc_; }
  [[nodiscard]] TreeLayerDesc&           GetDesc()             { return desc_; }
  [[nodiscard]] const std::vector<uint8_t>& GetDensityMap() const {
    return density_map_;
  }
  [[nodiscard]] std::vector<uint8_t>&    GetDensityMap()       {
    return density_map_;
  }
  [[nodiscard]] int GetMapWidth()  const { return map_width_;  }
  [[nodiscard]] int GetMapHeight() const { return map_height_; }

 private:
  TreeLayerDesc         desc_;
  int                   map_width_;
  int                   map_height_;
  std::vector<uint8_t>  density_map_;
  bool                  dirty_ = true;
};

}  // namespace environment
