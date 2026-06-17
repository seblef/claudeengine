#pragma once

#include "game/GameMesh.h"

namespace editor {

// Enqueues a depth-tested wireframe gizmo for mesh's physics shape.
// No-op if mesh has no PhysicsBodyDesc.
// Must be called between WireframeRenderer::BeginFrame() and WireframeRenderer::Render().
void EnqueuePhysicsGizmo(const game::GameMesh& mesh);

}  // namespace editor
