#pragma once

#include <vector>

#include "game/GameObject.h"

namespace editor {

// Enqueues two concentric wireframe spheres (inner = min_distance, outer =
// max_distance) for every kSoundEmitter object in objects.
//
// camera_distance is the orbit radius of the editor camera; it is used to
// enforce a minimum visible sphere size so gizmos remain legible when the
// camera is far from the emitter.
//
// selected_key must match the pointer passed to
// WireframeRenderer::SetHighlightedObject() so the selection highlight works.
void EnqueueSoundEmitterGizmos(const std::vector<game::GameObject*>& objects,
                                const void* selected_key,
                                float camera_distance);

}  // namespace editor
