#pragma once

#include <vector>

#include "game/GameObject.h"

namespace editor {

// Enqueues depth-tested wireframe cube overlays for every kGauge object.
// selected_key must match the pointer passed to WireframeRenderer::SetHighlightedObject().
void EnqueueGaugeGizmos(const std::vector<game::GameObject*>& objects,
                         const void* selected_key);

}  // namespace editor
