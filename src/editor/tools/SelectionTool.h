#pragma once

#include <imgui.h>

#include "editor/tools/EditorToolBase.h"
#include "editor/tools/PickingUtils.h"

namespace editor {

class EditorCommandHistory;
class EditorScene;

// Handles object selection, bounding-box display, and object deletion.
//
// Fires a multi-pass ray-cast on LMB release to select the nearest object:
// mesh (ray-triangle), light (screen-space proximity), player-start, and
// particle-system passes are evaluated in order. Draws an orange wireframe
// bounding box around the selected object each frame. Handles the Delete key
// to remove the selected object and push a DeleteObjectCommand.
class SelectionTool : public EditorToolBase {
 public:
  void OnActivate(const EditorToolContext& ctx) override;
  void OnDeactivate() override;
  void OnEvent(const core::Event& event) override;
  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;

 private:
  // Cached from OnActivate() for use in OnEvent(); cleared by OnDeactivate().
  EditorScene*          scene_   = nullptr;
  EditorCommandHistory* history_ = nullptr;
};

}  // namespace editor
