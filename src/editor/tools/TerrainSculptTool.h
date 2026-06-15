#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "editor/tools/EditorToolBase.h"

#include <imgui.h>

namespace abstract { class VideoDevice; }
namespace terrain {
class TerrainData;
class TerrainMaterial;
}  // namespace terrain

namespace editor {

class EditorCommandHistory;

// Tool that handles all terrain brush strokes (sculpt and paint) in the viewport.
//
// Owned by TerrainEditorPanel, which creates it in SetContext() and exposes it
// via GetSculptTool() so EditorViewport can activate it. The panel drives the
// brush mode and parameters through the setter API; all editing algorithms live
// here so the panel stays pure UI.
//
// Mouse input: LMB drag (without Alt) fires the active brush each frame;
// LMB release commits the stroke as an undoable command.
//
// Foliage mode is delegated back to TerrainEditorPanel via callbacks because
// foliage-specific context (GameTerrain, FoliageLayer) lives in the panel.
class TerrainSculptTool : public EditorToolBase {
 public:
  enum class Mode    { kSculpt, kPaint, kFoliage };
  enum class Tool    { kRaise, kLower, kSmooth, kFlatten };
  enum class Falloff { kLinear, kSmooth };

  // data, material, video, history: terrain context (not owned).
  // on_foliage_brush: fired each frame while LMB is held in Foliage mode.
  // on_foliage_end:   fired when LMB is released in Foliage mode.
  TerrainSculptTool(terrain::TerrainData* data,
                    terrain::TerrainMaterial* material,
                    abstract::VideoDevice* video,
                    EditorCommandHistory* history,
                    std::function<void(float, float, bool, float)> on_foliage_brush,
                    std::function<void()> on_foliage_end);

  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;

  // Returns true while any stroke is active so selection does not also fire.
  bool IsCapturingMouse() const override;

  // ---- Mode and parameter setters (called by TerrainEditorPanel) -------------
  void SetMode(Mode m)          { mode_          = m; }
  void SetTool(Tool t)          { tool_          = t; }
  void SetFalloff(Falloff f)    { falloff_       = f; }
  void SetRadius(float r)       { radius_        = r; }
  void SetStrength(float s)     { strength_      = s; }
  void SetActiveLayer(int l)    { active_layer_  = l; }
  void SetPaintOpacity(float o) { paint_opacity_ = o; }

  [[nodiscard]] Mode    GetMode()         const { return mode_; }
  [[nodiscard]] Tool    GetTool()         const { return tool_; }
  [[nodiscard]] Falloff GetFalloff()      const { return falloff_; }
  [[nodiscard]] float   GetRadius()       const { return radius_; }
  [[nodiscard]] float   GetStrength()     const { return strength_; }
  [[nodiscard]] int     GetActiveLayer()  const { return active_layer_; }
  [[nodiscard]] float   GetPaintOpacity() const { return paint_opacity_; }

 private:
  [[nodiscard]] float ComputeFalloff(float t) const;

  void EnsureRegionCovers(int nx0, int nz0, int nx1, int nz1);
  void ApplyBrushFootprint(float cx_world, float cz_world, float dt);
  void OnSculptAt(float wx, float wz, bool first_touch, float dt);
  void OnSculptEnd();

  void EnsurePaintRegionCovers(int nx0, int nz0, int nx1, int nz1);
  void ApplyPaintFootprint(float cx_world, float cz_world);
  void OnPaintAt(float wx, float wz, bool first_touch);
  void OnPaintEnd();

  // ---- Context (not owned) ---------------------------------------------------
  // cppcheck-suppress unusedStructMember
  terrain::TerrainData*     data_     = nullptr;
  // cppcheck-suppress unusedStructMember
  terrain::TerrainMaterial* material_ = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*    video_    = nullptr;
  // cppcheck-suppress unusedStructMember
  EditorCommandHistory*     history_  = nullptr;

  // ---- Foliage-mode callbacks ------------------------------------------------
  // cppcheck-suppress unusedStructMember
  std::function<void(float, float, bool, float)> on_foliage_brush_;
  // cppcheck-suppress unusedStructMember
  std::function<void()>                          on_foliage_end_;

  // ---- Mode ------------------------------------------------------------------
  // cppcheck-suppress unusedStructMember
  Mode mode_ = Mode::kSculpt;

  // ---- Shared brush parameters -----------------------------------------------
  // cppcheck-suppress unusedStructMember
  Falloff falloff_ = Falloff::kSmooth;
  // cppcheck-suppress unusedStructMember
  float   radius_  = 10.f;

  // ---- Sculpt parameters -----------------------------------------------------
  // cppcheck-suppress unusedStructMember
  Tool  tool_     = Tool::kRaise;
  // cppcheck-suppress unusedStructMember
  float strength_ = 0.5f;

  // ---- Paint parameters ------------------------------------------------------
  // cppcheck-suppress unusedStructMember
  int   active_layer_  = 0;
  // cppcheck-suppress unusedStructMember
  float paint_opacity_ = 0.5f;

  // ---- Sculpt stroke state ---------------------------------------------------
  // cppcheck-suppress unusedStructMember
  bool     stroke_active_    = false;
  // cppcheck-suppress unusedStructMember
  int      stroke_x0_        = 0;
  // cppcheck-suppress unusedStructMember
  int      stroke_z0_        = 0;
  // cppcheck-suppress unusedStructMember
  int      stroke_x1_        = 0;
  // cppcheck-suppress unusedStructMember
  int      stroke_z1_        = 0;
  // cppcheck-suppress unusedStructMember
  float    stroke_flatten_h_ = 0.f;
  // cppcheck-suppress unusedStructMember
  std::vector<uint16_t> stroke_pre_snapshot_;

  // ---- Paint stroke state ----------------------------------------------------
  // cppcheck-suppress unusedStructMember
  bool                 paint_stroke_active_ = false;
  // cppcheck-suppress unusedStructMember
  int                  paint_x0_            = 0;
  // cppcheck-suppress unusedStructMember
  int                  paint_z0_            = 0;
  // cppcheck-suppress unusedStructMember
  int                  paint_x1_            = 0;
  // cppcheck-suppress unusedStructMember
  int                  paint_z1_            = 0;
  // cppcheck-suppress unusedStructMember
  std::vector<uint8_t> paint_pre_snapshot_;

  // ---- Foliage stroke state --------------------------------------------------
  // cppcheck-suppress unusedStructMember
  bool foliage_stroke_active_ = false;
};

}  // namespace editor
