#pragma once

#include <vector>

#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/VertexBase.h"
#include "game/GameObject.h"

namespace editor {

// Renders a wireframe sphere gizmo at each kParticleSystem object's world
// position without depth testing, so the marker is always visible.
//
// Sphere radius is taken from the first sub-system's emitter_radius, unless
// that sub-system's shape is kPoint — in which case 0.5 m is used.
//
// Each frame: BuildVertices() builds the CPU line list from the scene objects,
// then Render() uploads it and issues a single non-indexed draw call with depth
// testing disabled.
class ParticleGizmoRenderer {
 public:
  // video must outlive this renderer.
  explicit ParticleGizmoRenderer(abstract::VideoDevice* video);
  ~ParticleGizmoRenderer();

  ParticleGizmoRenderer(const ParticleGizmoRenderer&)            = delete;
  ParticleGizmoRenderer& operator=(const ParticleGizmoRenderer&) = delete;

  // Draws a wireframe sphere gizmo for every kParticleSystem object in
  // `objects` into `fbo`. `selected` may be nullptr; the selected particle
  // system is highlighted in a brighter colour.
  void Render(const std::vector<game::GameObject*>& objects,
              const game::GameObject* selected,
              abstract::RenderTargetGroup* fbo);

 private:
  // Appends a line segment p0→p1 with `color` to vertices_.
  void PushSegment(const core::Vec3f& p0, const core::Vec3f& p1,
                   const core::Color& color);

  // Appends circle segments (kCircleSegments lines) for a circle with `center`,
  // `radius`, described by two orthogonal axis vectors `ax` and `ay`.
  void PushCircle(const core::Vec3f& center, float radius,
                  const core::Vec3f& ax, const core::Vec3f& ay,
                  const core::Color& color);

  // Populates vertices_ with three orthogonal circle rings per particle system.
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

  // cppcheck-suppress unusedStructMember
  static constexpr int   kCircleSegments  = 32;
  // cppcheck-suppress unusedStructMember
  static constexpr float kDefaultRadius   = 0.5f;
};

}  // namespace editor
