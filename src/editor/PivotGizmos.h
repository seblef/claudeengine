#pragma once

#include <vector>

#include "game/GameObject.h"

namespace editor {

// Enqueues a 3-axis cross overlay for every kPivot object.
//
// The three arms are drawn in local space (X=red, Y=green, Z=blue) so the
// pivot's orientation is visible. selected_key must match the pointer passed to
// WireframeRenderer::SetHighlightedObject() so the selection highlight works.
void EnqueuePivotGizmos(const std::vector<game::GameObject*>& objects,
                         const void* selected_key);

}  // namespace editor
