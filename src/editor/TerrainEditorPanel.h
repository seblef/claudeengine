#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "terrain/TerrainGenerator.h"

namespace abstract { class VideoDevice; }
namespace game { class GameTerrain; }
namespace terrain {
class TerrainData;
class TerrainMaterial;
}  // namespace terrain


namespace editor {

class EditorCommandHistory;
class EditorToolBase;
class TerrainPainterWindow;
class TerrainSculptTool;

// Dockable ImGui panel for terrain sculpting and texture painting.
//
// Owns a TerrainSculptTool which handles all brush algorithms (sculpt,
// paint, foliage). The panel is pure UI: it reads/writes tool parameters
// via the tool's getter/setter API and exposes GetSculptTool() so
// EditorWindow can activate the tool in the viewport.
class TerrainEditorPanel {
 public:
  TerrainEditorPanel();
  ~TerrainEditorPanel();

  // Renders the panel body (tab bar + active tab controls).
  // Must be called inside an ImGui::Begin/End block.
  void Render();

  // Opens the standalone terrain import floating window.
  // Must be called outside any ImGui::Begin/End block.
  void OpenImportWindow();

  // Renders the terrain import floating window when open.
  // Must be called outside any ImGui::Begin/End block each frame.
  void RenderImportWindow();

  // Opens the procedural terrain generation floating window.
  // Must be called outside any ImGui::Begin/End block.
  void OpenGenerateWindow();

  // Renders the procedural terrain generation floating window when open.
  // Must be called outside any ImGui::Begin/End block each frame.
  void RenderGenerateWindow();

  // Renders the auto-paint floating window when open.
  // Must be called outside any ImGui::Begin/End block each frame.
  void RenderPainterWindow();

  // Provides the editing context. Call when a terrain is added/loaded;
  // pass nullptr data to reset. history must outlive this panel.
  // terrain_obj is used by the Foliage tab to manage foliage layers.
  void SetContext(terrain::TerrainData* data,
                  terrain::TerrainMaterial* material,
                  abstract::VideoDevice* video,
                  EditorCommandHistory* history,
                  game::GameTerrain* terrain_obj = nullptr);

  // Returns the owned TerrainSculptTool so EditorWindow can set it as
  // the viewport's active base tool. Returns nullptr if no context is set.
  [[nodiscard]] EditorToolBase* GetSculptTool();

  // Returns true when a context is set (terrain exists in scene).
  [[nodiscard]] bool IsActive() const { return data_ != nullptr; }

  // Sets a callback invoked when the user imports a heightmap with no terrain
  // currently in the scene. The callback receives the decoded uint16 samples,
  // terrain dimensions (w, h), and the mapped height range [min_h, max_h].
  // EditorWindow uses this to create and register a new terrain.
  void SetOnCreateFromImport(
      std::function<void(std::vector<uint16_t>, int, int, float, float)> cb);

  // Sets a callback invoked whenever foliage data is modified (brush stroke
  // completed, layer added/removed, or layer settings changed).
  // EditorWindow uses this to mark the scene dirty and enable the Save button.
  void SetOnFoliageModified(std::function<void()> cb) {
    on_foliage_modified_ = std::move(cb);
  }

 private:
  enum class FoliageBrushMode { kPaint, kErase };
  enum class ActiveTab { kSculpt, kPaint, kMaterial, kProperties, kExport, kFoliage };
  enum class IoState  { kIdle, kConfirmResize };

  // ---- Tab render -----------------------------------------------------------
  void RenderSculptTab();
  void RenderPaintTab();
  void RenderMaterialTab();
  void RenderPropertiesTab();
  void RenderExportTab();
  void RenderFoliageTab();

  // Renders the resize-confirmation modal and status message for import ops.
  // Called both from RenderImportWindow() and can be shared with export output.
  void RenderImportStatusAndModal();

  // Opens an NFD texture file dialog starting in data/textures/ and returns
  // the relative path (relative to data/textures/) on success, or an empty
  // string if the user cancelled or the selected file is outside that root.
  [[nodiscard]] static std::string BrowseTexture();

  // Opens an NFD mesh file dialog starting in data/ and returns the path
  // relative to data/ on success, or empty on cancel / out-of-root.
  [[nodiscard]] static std::string BrowseMesh();

  // ---- Import / Export helpers ----------------------------------------------

  // Loads a PNG heightmap and either applies it immediately (same dimensions)
  // or sets the pending-resize state (different dimensions).
  // Returns false and sets io_status_msg_ on load error.
  bool LoadAndApplyPNG(const std::string& path);

  // Loads an HDR heightmap (via stbi_loadf) or a true OpenEXR file (via
  // TinyEXR), dispatching on file extension. Float pixel values are expected
  // in [0, 1] and mapped to [import_min_h_, import_max_h_]. Either applies
  // immediately or sets the pending-resize state. Returns false on error.
  bool LoadAndApplyHDR(const std::string& path);

  // Applies an already-decoded heightmap to the terrain and updates the height
  // range to [min_h, max_h]. If needs_resize is true the terrain is resized
  // and the splatmap reset.
  void ApplyImportedHeightmap(const std::vector<uint16_t>& data, int w, int h,
                              bool needs_resize, float min_h, float max_h);

  // Writes the current heightmap as an 8-bit grayscale PNG.
  void DoExportPNG(const std::string& path);

  // Writes the current heightmap as a raw R16 (uint16_t) binary file.
  void DoExportR16(const std::string& path);

  // ---- Active tab -----------------------------------------------------------
  ActiveTab active_tab_ = ActiveTab::kSculpt;

  // ---- Import / Export state ------------------------------------------------
  float  import_min_h_  = 0.f;
  float  import_max_h_  = 100.f;

  bool    show_import_window_   = false;

  // ---- Generate window state ------------------------------------------------
  enum class GenAlgorithm { kFbm, kRidged };

  bool         show_generate_window_  = false;
  GenAlgorithm gen_algorithm_         = GenAlgorithm::kFbm;
  float        gen_world_width_       = 1024.f;
  float        gen_world_depth_       = 1024.f;
  float        gen_meters_per_texel_  = 1.f;
  float        gen_min_h_             = 0.f;
  float        gen_max_h_             = 100.f;
  // cppcheck-suppress unusedStructMember
  terrain::FbmParams      gen_fbm_params_;
  // cppcheck-suppress unusedStructMember
  terrain::RidgedParams   gen_ridged_params_;
  bool                    gen_erosion_enabled_  = false;
  // cppcheck-suppress unusedStructMember
  terrain::ErosionParams  gen_erosion_params_;
  // cppcheck-suppress unusedStructMember
  std::function<void(std::vector<uint16_t>, int, int, float, float)>
      on_create_from_import_;
  // cppcheck-suppress unusedStructMember
  std::function<void()> on_foliage_modified_;
  IoState io_state_           = IoState::kIdle;
  int     io_pending_w_     = 0;
  int     io_pending_h_     = 0;
  // cppcheck-suppress unusedStructMember
  float   io_pending_min_h_ = 0.f;
  // cppcheck-suppress unusedStructMember
  float   io_pending_max_h_ = 100.f;
  // cppcheck-suppress unusedStructMember
  std::vector<uint16_t> io_pending_data_;
  // cppcheck-suppress unusedStructMember
  std::string io_status_msg_;
  bool        io_status_ok_ = true;

  // ---- Foliage tab parameters -----------------------------------------------
  FoliageBrushMode foliage_brush_mode_    = FoliageBrushMode::kPaint;
  int              foliage_active_layer_  = 0;
  float            foliage_radius_        = 5.f;
  float            foliage_strength_      = 0.5f;
  // cppcheck-suppress unusedStructMember
  bool             foliage_stroke_active_ = false;

  // ---- Context --------------------------------------------------------------
  terrain::TerrainData*      data_         = nullptr;
  terrain::TerrainMaterial*  material_     = nullptr;
  abstract::VideoDevice*     video_        = nullptr;
  EditorCommandHistory*      history_      = nullptr;
  // cppcheck-suppress unusedStructMember
  game::GameTerrain*         terrain_obj_  = nullptr;

  // ---- Sculpt tool (owned; null until SetContext() receives non-null data) --
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<TerrainSculptTool> sculpt_tool_;

  // ---- Auto-paint floating window -------------------------------------------
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<TerrainPainterWindow> painter_window_;
};

}  // namespace editor
