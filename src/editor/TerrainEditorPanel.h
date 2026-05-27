#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace abstract { class VideoDevice; }
namespace terrain {
class TerrainData;
class TerrainMaterial;
class TerrainNormalMap;
}  // namespace terrain

namespace editor {

class EditorCommandHistory;

// Dockable ImGui panel for terrain sculpting and texture painting.
//
// Exposes two tabs — **Sculpt** and **Paint** — that share the same
// radius and falloff brush parameters but use separate tool-specific
// controls (tool/strength for Sculpt; layer/opacity for Paint).
//
// EditorWindow calls OnBrushAt() each frame while LMB is held in the
// viewport, then OnBrushEnd() when the button is released. The active
// tab determines whether the stroke modifies the heightmap (sculpt) or
// the splatmap (paint). At stroke end a SculptBrushCommand or
// PaintBrushCommand is pushed to the history for undo/redo.
//
// Sculpt tools:
//   Raise   — h += strength * w * dt * kBrushRate
//   Lower   — h -= strength * w * dt * kBrushRate
//   Smooth  — blend toward 3×3 neighbourhood average
//   Flatten — blend toward the height sampled on first brush touch
//
// Paint brush:
//   Increases the active layer's splatmap channel by opacity * w per
//   frame, then renormalises all four channels to sum to 1.
//
// Falloff modes (shared):
//   Linear — w = 1 - t         (t = dist / radius, t in [0,1])
//   Smooth — w = smoothstep(0, 1, 1 - t)
class TerrainEditorPanel {
 public:
  TerrainEditorPanel() = default;

  // Renders the panel body (tab bar + active tab controls).
  // Must be called inside an ImGui::Begin/End block.
  void Render();

  // Provides the editing context. Call when a terrain is added/loaded;
  // pass nullptr data to reset. history must outlive this panel.
  void SetContext(terrain::TerrainData* data,
                  terrain::TerrainMaterial* material,
                  terrain::TerrainNormalMap* normal_map,
                  abstract::VideoDevice* video,
                  EditorCommandHistory* history);

  // Notifies the panel that the brush is at world (wx, wz) this frame.
  // first_touch must be true on the first frame of a new drag stroke.
  // dt is the frame delta in seconds (used by sculpt tools).
  void OnBrushAt(float wx, float wz, bool first_touch, float dt);

  // Notifies the panel that the drag stroke has ended (LMB released).
  // Pushes the appropriate command to the command history.
  void OnBrushEnd();

  // Returns true when a context is set (terrain exists in scene).
  [[nodiscard]] bool IsActive() const { return data_ != nullptr; }

 private:
  enum class Tool    { kRaise, kLower, kSmooth, kFlatten };
  enum class Falloff { kLinear, kSmooth };
  enum class ActiveTab { kSculpt, kPaint, kMaterial, kProperties };

  // Returns the falloff weight for normalised distance t in [0, 1].
  [[nodiscard]] float ComputeFalloff(float t) const;

  // ---- Sculpt helpers -------------------------------------------------------

  // Expands the tracked pre-snapshot region to cover [nx0, nz0, nx1, nz1).
  // Newly added texels are snapshotted from the current heightmap.
  void EnsureRegionCovers(int nx0, int nz0, int nx1, int nz1);

  // Applies the active sculpt tool to all texels within radius_ of the
  // world position (cx_world, cz_world). Uploads the affected GPU tile.
  void ApplyBrushFootprint(float cx_world, float cz_world, float dt);

  // Sculpt-specific stroke entry points.
  void OnSculptAt(float wx, float wz, bool first_touch, float dt);
  void OnSculptEnd();

  // ---- Paint helpers --------------------------------------------------------

  // Expands the splatmap pre-snapshot region to cover [nx0, nz0, nx1, nz1).
  // Newly added texels are snapshotted from the current splatmap.
  void EnsurePaintRegionCovers(int nx0, int nz0, int nx1, int nz1);

  // Increases the active layer for all texels within radius_ of
  // (cx_world, cz_world), renormalises all channels, uploads the tile.
  void ApplyPaintFootprint(float cx_world, float cz_world);

  // Paint-specific stroke entry points.
  void OnPaintAt(float wx, float wz, bool first_touch);
  void OnPaintEnd();

  // ---- Tab render -----------------------------------------------------------
  void RenderSculptTab();
  void RenderPaintTab();
  void RenderMaterialTab();
  void RenderPropertiesTab();

  // Opens an NFD texture file dialog starting in data/textures/ and returns
  // the relative path (relative to data/textures/) on success, or an empty
  // string if the user cancelled or the selected file is outside that root.
  [[nodiscard]] static std::string BrowseTexture();

  // ---- Shared brush parameters ----------------------------------------------
  Falloff falloff_  = Falloff::kSmooth;
  float   radius_   = 10.f;

  // ---- Sculpt-specific parameters -------------------------------------------
  Tool  tool_     = Tool::kRaise;
  float strength_ = 0.5f;

  // ---- Paint-specific parameters --------------------------------------------
  int   active_layer_  = 0;
  float paint_opacity_ = 0.5f;

  // ---- Active tab -----------------------------------------------------------
  ActiveTab active_tab_ = ActiveTab::kSculpt;

  // ---- Context --------------------------------------------------------------
  terrain::TerrainData*      data_       = nullptr;
  terrain::TerrainMaterial*  material_   = nullptr;
  terrain::TerrainNormalMap* normal_map_ = nullptr;
  abstract::VideoDevice*     video_      = nullptr;
  EditorCommandHistory*      history_    = nullptr;

  // ---- Sculpt stroke state --------------------------------------------------
  bool     stroke_active_    = false;
  int      stroke_x0_        = 0;
  int      stroke_z0_        = 0;
  int      stroke_x1_        = 0;
  int      stroke_z1_        = 0;
  float    stroke_flatten_h_ = 0.f;
  // cppcheck-suppress unusedStructMember
  std::vector<uint16_t> stroke_pre_snapshot_;

  // ---- Paint stroke state ---------------------------------------------------
  // cppcheck-suppress unusedStructMember
  bool                 paint_stroke_active_ = false;
  int                  paint_x0_            = 0;
  int                  paint_z0_            = 0;
  int                  paint_x1_            = 0;
  int                  paint_z1_            = 0;
  // cppcheck-suppress unusedStructMember
  std::vector<uint8_t> paint_pre_snapshot_;
};

}  // namespace editor
