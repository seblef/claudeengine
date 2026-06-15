#pragma once

#include <functional>

#include "editor/tools/EditorToolBase.h"

#include <imgui.h>

namespace terrain { class TerrainData; }

namespace editor {

// Tool that handles terrain sculpt brush strokes in the viewport.
//
// Constructed by EditorWindow with the TerrainEditorPanel's OnBrushAt / OnBrushEnd
// callbacks. While active, LMB drag (without Alt) fires on_brush_ with the
// terrain world XZ hit point and a first_touch flag; LMB release fires on_end_.
//
// IsCapturingMouse() returns true during an active stroke so that other tools
// (e.g. SelectionTool) do not also fire on the same mouse event.
class TerrainSculptTool : public EditorToolBase {
 public:
  // on_brush: fired each frame while LMB is held; args: world_x, world_z, first_touch, dt.
  // on_end:   fired when the stroke ends (LMB released).
  // data:     terrain heightmap used for ray intersection (not owned).
  TerrainSculptTool(std::function<void(float, float, bool, float)> on_brush,
                    std::function<void()> on_end,
                    const terrain::TerrainData* data);

  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;

  bool IsCapturingMouse() const override { return stroke_active_; }

 private:
  // cppcheck-suppress unusedStructMember
  std::function<void(float, float, bool, float)> on_brush_;
  // cppcheck-suppress unusedStructMember
  std::function<void()>                          on_end_;
  // cppcheck-suppress unusedStructMember
  const terrain::TerrainData*                    data_;
  // cppcheck-suppress unusedStructMember
  bool                                           stroke_active_ = false;
};

}  // namespace editor
