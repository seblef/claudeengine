#pragma once

#include <imgui.h>

namespace editor {

struct EditorToolContext;

// Casts a picking ray from mouse_pos through the viewport and updates the
// scene selection. Evaluates four passes in order: mesh (ray-triangle), light
// (screen-space proximity), player-start, and particle-system.
//
// ctrl_held — when true the hit object is toggled in/out of the existing
// selection; when false the selection is replaced with the single hit (or
// cleared on a miss).
void PickObjectAt(const EditorToolContext& ctx, ImVec2 mouse_pos,
                  ImVec2 image_pos, ImVec2 image_size, bool ctrl_held = false);

// Draws orange wireframe bounding boxes around all selected objects.
// dl must be a valid ImDrawList. No-op when nothing is selected.
void DrawSelectedBBox(const EditorToolContext& ctx, ImDrawList* dl,
                      ImVec2 image_pos, ImVec2 image_size);

}  // namespace editor
