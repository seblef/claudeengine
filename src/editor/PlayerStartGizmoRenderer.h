#pragma once

#include <vector>

#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/VertexBase.h"
#include "game/GameObject.h"

namespace editor {

// Renders the player-start flag gizmo as world-space line primitives without
// depth testing, so the marker is always visible regardless of terrain or meshes
// in front of it.
//
// Geometry per player-start object:
//   - Vertical white pole from the world position up 2 units.
//   - Bright-green (#00CC44) triangular flag at the top of the pole.
//
// Each frame: BuildVertices() builds the CPU line list from the scene objects,
// then Render() uploads it and issues a single non-indexed draw call with depth
// testing disabled.
class PlayerStartGizmoRenderer {
 public:
  // video must outlive this renderer.
  explicit PlayerStartGizmoRenderer(abstract::VideoDevice* video);
  ~PlayerStartGizmoRenderer();

  PlayerStartGizmoRenderer(const PlayerStartGizmoRenderer&)            = delete;
  PlayerStartGizmoRenderer& operator=(const PlayerStartGizmoRenderer&) = delete;

  // Draws a flag gizmo for every kPlayerStart object in `objects` into `fbo`.
  // fbo must be the plain colour render target (no depth attachment) so that
  // depth testing is skipped and the gizmo is always on top.
  // `selected` may be nullptr; the selected player start is highlighted brighter.
  void Render(const std::vector<game::GameObject*>& objects,
              const game::GameObject* selected,
              abstract::RenderTargetGroup* fbo);

 private:
  // Appends a line segment p0→p1 with `color` to vertices_.
  void PushSegment(const core::Vec3f& p0, const core::Vec3f& p1,
                   const core::Color& color);

  // Populates vertices_ with pole and flag segments for every player-start.
  void BuildVertices(const std::vector<game::GameObject*>& objects,
                     const game::GameObject* selected);

  // Reallocates the GPU vertex buffer if vertices_ exceeds its capacity.
  void EnsureVertexBuffer();

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                  video_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                       shader_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::VertexBuffer> vertex_buf_;
  // cppcheck-suppress unusedStructMember
  int                                     vertex_buf_capacity_ = 0;

  // cppcheck-suppress unusedStructMember
  std::vector<core::VertexBase> vertices_;
};

}  // namespace editor
