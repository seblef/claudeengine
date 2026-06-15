#include "editor/tools/TerrainSculptTool.h"

#include <utility>

#include "editor/tools/EditorToolContext.h"
#include "editor/tools/ViewportRaycast.h"

#include <imgui.h>

namespace editor {

TerrainSculptTool::TerrainSculptTool(
    std::function<void(float, float, bool, float)> on_brush,
    std::function<void()> on_end,
    const terrain::TerrainData* data)
    : on_brush_(std::move(on_brush)),
      on_end_(std::move(on_end)),
      data_(data) {}

void TerrainSculptTool::OnRender(const EditorToolContext& ctx,
                                  ImVec2 image_pos, ImVec2 image_size) {
  if (!ImGui::IsWindowHovered()) return;

  const bool lmb_down = ImGui::IsMouseDown(ImGuiMouseButton_Left)
                        && !ImGui::GetIO().KeyAlt;

  if (lmb_down) {
    const auto hit = ComputeTerrainHit(ctx.camera, data_,
                                        ImGui::GetMousePos(), image_pos, image_size);
    if (hit && on_brush_) {
      on_brush_(hit->x, hit->z, !stroke_active_, ImGui::GetIO().DeltaTime);
      stroke_active_ = true;
    }
  } else if (stroke_active_) {
    stroke_active_ = false;
    if (on_end_) on_end_();
  }
}

}  // namespace editor
