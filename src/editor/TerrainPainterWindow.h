#pragma once

#include <string>
#include <vector>

namespace abstract { class VideoDevice; }
namespace terrain {
class TerrainData;
class TerrainMaterial;
}  // namespace terrain

namespace editor {

class EditorCommandHistory;

// Floating window that auto-paints the entire terrain splatmap by height.
//
// The user defines a list of HeightRange entries — each associating a terrain
// material layer index with a [min_height, max_height] world-space interval.
// When "Apply" is clicked every splatmap texel is re-painted: the texel's
// world height (plus a deterministic noise perturbation) is matched against
// the entries, producing blend weights that are normalised and written to the
// splatmap.  The noise_level parameter controls the amplitude of that
// perturbation, which widens and randomises layer boundaries so they look
// natural rather than perfectly horizontal.
//
// The full operation is pushed as a single PaintBrushCommand so it can be
// undone in one step.
//
// Usage:
//   Call Open() to show the window.
//   Call SetContext() whenever the terrain changes.
//   Call Render() every frame outside any ImGui::Begin/End block.
class TerrainPainterWindow {
 public:
  TerrainPainterWindow() = default;

  // Shows the window.
  void Open();

  // Renders the window (no-op when closed). Must be called outside any
  // ImGui::Begin/End block.
  void Render();

  // Provides access to the terrain data, splatmap and command history.
  // Pass nullptr pointers to reset (terrain removed from scene).
  void SetContext(terrain::TerrainData* data,
                  terrain::TerrainMaterial* material,
                  abstract::VideoDevice* video,
                  EditorCommandHistory* history);

 private:
  // One entry mapping a height interval to a material layer.
  struct HeightRange {
    int   layer      = 0;
    float min_height = 0.f;
    float max_height = 100.f;
  };

  // Paints every splatmap texel based on the current ranges and noise level.
  // Snapshots the splatmap before and after and pushes a PaintBrushCommand.
  void ApplyPainting();

  // Returns a deterministic pseudo-random value in [-1, 1] for texel (x, z).
  static float Noise(int x, int z);

  bool show_ = false;

  // cppcheck-suppress unusedStructMember
  std::vector<HeightRange> ranges_;
  float noise_level_ = 0.1f;

  // cppcheck-suppress unusedStructMember
  std::string status_msg_;
  bool        status_ok_ = true;

  terrain::TerrainData*     data_     = nullptr;
  terrain::TerrainMaterial* material_ = nullptr;
  abstract::VideoDevice*    video_    = nullptr;
  // cppcheck-suppress unusedStructMember
  EditorCommandHistory*     history_  = nullptr;
};

}  // namespace editor
