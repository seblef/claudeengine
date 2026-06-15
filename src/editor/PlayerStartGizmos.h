#pragma once

#include <vector>

#include "game/GameObject.h"

namespace editor {

// Enqueues pole-and-flag overlay wireframes for every kPlayerStart object.
// selected_key must match the pointer passed to WireframeRenderer::SetHighlightedObject().
void EnqueuePlayerStartGizmos(const std::vector<game::GameObject*>& objects,
                               const void* selected_key);

}  // namespace editor
