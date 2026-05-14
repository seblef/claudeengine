#pragma once

#include <memory>
#include <vector>

#include "abstract/Shader.h"
#include "abstract/VideoDevice.h"
#include "renderer/GeometryData.h"

namespace renderer {

class Light;

// Renders shadow-map thumbnails as an overlay on the left side of the screen.
//
// Cycling (CycleNext) advances through one entry per light that owns an active
// shadow map:
//   - GlobalLight  → 4 cascade tiles stacked vertically.
//   - Spot light   → 1 tile.
//   - Omni light   → 6 cube-face tiles arranged in a 2-column grid.
//
// Index 0 means "off".  Wraps around after the last active shadow map.
// Depth textures are temporarily put in raw-sampling mode (compare mode OFF)
// during visualization and restored immediately after.
class ShadowDebugRenderer {
 public:
  explicit ShadowDebugRenderer(abstract::VideoDevice* video);
  ~ShadowDebugRenderer();

  ShadowDebugRenderer(const ShadowDebugRenderer&)            = delete;
  ShadowDebugRenderer& operator=(const ShadowDebugRenderer&) = delete;

  // Advances to the next shadow-map entry; wraps to 0 (off) after the last.
  void CycleNext(const std::vector<Light*>& lights);

  // Renders thumbnails for the current entry to the bound framebuffer.
  // Disables depth test/write for the overlay; the caller must restore state.
  void Render(const std::vector<Light*>& lights);

  [[nodiscard]] bool IsActive() const { return current_index_ != 0; }

 private:
  // Per-entry type tag for the rendering switch.
  enum class EntryType { kCSM, kSpot, kOmni };

  struct Entry {
    // cppcheck-suppress unusedStructMember
    EntryType type;
    // cppcheck-suppress unusedStructMember
    const Light* light = nullptr;
  };

  // Collects the current frame's valid shadow-map entries.
  std::vector<Entry> CollectEntries(const std::vector<Light*>& lights) const;

  // Renders one 2D depth tile at the given NDC bounds.
  void RenderTile2D(abstract::RenderTarget* rt,
                    float x0, float y0, float x1, float y1);

  // Renders one cube-face tile at the given NDC bounds.
  void RenderTileCube(abstract::RenderTargetCube* rt, int face,
                      float x0, float y0, float x1, float y1);

  // Tile dimensions in NDC — 200 px square at the current resolution.
  [[nodiscard]] float TileW() const;
  [[nodiscard]] float TileH() const;

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             shader_2d_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             shader_cube_;
  std::unique_ptr<GeometryData> quad_;

  // 0 = off; 1..N = index into CollectEntries().
  int current_index_ = 0;
};

}  // namespace renderer
