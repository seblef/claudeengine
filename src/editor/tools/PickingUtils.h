#pragma once

#include <imgui.h>

namespace editor {

struct EditorToolContext;

// Casts a picking ray from mouse_pos through the viewport and sets the scene
// selection to the nearest hit object (or nullptr on a miss). Evaluates four
// passes in order: mesh (ray-triangle), light (screen-space proximity against
// wireframe geometry), player-start, and particle-system.
void PickObjectAt(const EditorToolContext& ctx, ImVec2 mouse_pos,
                  ImVec2 image_pos, ImVec2 image_size);

// Draws an orange wireframe bounding box around the scene's selected object.
// dl must be a valid ImDrawList. Assumes ctx.scene->GetSelectedObject() != nullptr.
void DrawSelectedBBox(const EditorToolContext& ctx, ImDrawList* dl,
                      ImVec2 image_pos, ImVec2 image_size);

}  // namespace editor
