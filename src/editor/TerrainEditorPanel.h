#pragma once

#include <cstdint>
#include <vector>

namespace abstract { class VideoDevice; }
namespace terrain {
class TerrainData;
class TerrainNormalMap;
}  // namespace terrain

namespace editor {

class EditorCommandHistory;

// Dockable ImGui panel for terrain sculpting.
//
// Renders brush parameter controls (tool, radius, strength, falloff) and
// manages the sculpt stroke lifecycle. EditorWindow calls OnBrushAt() each
// frame while LMB is held in the viewport, then OnBrushEnd() when the button
// is released. At stroke end, a SculptBrushCommand is pushed to the history
// for undo/redo.
//
// Brush tools:
//   Raise   — h += strength * w * dt * kBrushRate
//   Lower   — h -= strength * w * dt * kBrushRate
//   Smooth  — blend toward 3×3 neighbourhood average
//   Flatten — blend toward the height sampled on first brush touch
//
// Falloff modes:
//   Linear — w = 1 - t         (t = dist / radius, t in [0,1])
//   Smooth — w = smoothstep(0, 1, 1 - t)
class TerrainEditorPanel {
 public:
  TerrainEditorPanel() = default;

  // Renders the panel body. Must be called inside an ImGui::Begin/End block.
  void Render();

  // Provides the sculpt context. Call when a terrain is added/loaded; pass
  // nullptr data to reset. history must outlive this panel.
  void SetContext(terrain::TerrainData* data,
                  terrain::TerrainNormalMap* normal_map,
                  abstract::VideoDevice* video,
                  EditorCommandHistory* history);

  // Notifies the panel that the sculpt brush is at world (wx, wz) this frame.
  // first_touch must be true on the first frame of a new drag stroke.
  // dt is the frame delta in seconds.
  void OnBrushAt(float wx, float wz, bool first_touch, float dt);

  // Notifies the panel that the drag stroke has ended (LMB released).
  // Pushes a SculptBrushCommand to the command history.
  void OnBrushEnd();

  // Returns true when sculpt context is set (terrain exists in scene).
  [[nodiscard]] bool IsActive() const { return data_ != nullptr; }

 private:
  enum class Tool { kRaise, kLower, kSmooth, kFlatten };
  enum class Falloff { kLinear, kSmooth };

  // Returns the falloff weight for normalised distance t in [0, 1].
  [[nodiscard]] float ComputeFalloff(float t) const;

  // Expands the tracked pre-snapshot region to cover [nx0, nz0, nx1, nz1).
  // Newly added texels are snapshotted from the current (unmodified) data.
  void EnsureRegionCovers(int nx0, int nz0, int nx1, int nz1);

  // Applies the active brush to all texels within radius_ of (cx_world, cz_world).
  // Updates TerrainData, GPU heightmap, and normal map for the brush footprint.
  void ApplyBrushFootprint(float cx_world, float cz_world, float dt);

  Tool    tool_     = Tool::kRaise;
  Falloff falloff_  = Falloff::kSmooth;
  float   radius_   = 10.f;
  float   strength_ = 0.5f;

  terrain::TerrainData*      data_       = nullptr;
  terrain::TerrainNormalMap* normal_map_ = nullptr;
  abstract::VideoDevice*     video_      = nullptr;
  EditorCommandHistory*      history_    = nullptr;

  // Stroke state — active from OnBrushAt(first_touch=true) to OnBrushEnd().
  bool     stroke_active_    = false;
  int      stroke_x0_        = 0;
  int      stroke_z0_        = 0;
  int      stroke_x1_        = 0;
  int      stroke_z1_        = 0;
  float    stroke_flatten_h_ = 0.f;
  // cppcheck-suppress unusedStructMember
  std::vector<uint16_t> stroke_pre_snapshot_;
};

}  // namespace editor
