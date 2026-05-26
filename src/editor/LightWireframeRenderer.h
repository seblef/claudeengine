#pragma once

#include <vector>

#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/VertexBase.h"
#include "game/GameLight.h"
#include "game/GameObject.h"

namespace editor {

// Renders editor light wireframes as world-space line primitives into a depth-
// aware render target, so that lights behind opaque geometry are occluded.
//
// Each frame: BuildVertices() populates the CPU-side line list from the scene
// objects, then Render() uploads the data and issues a single non-indexed draw
// call using the light_wireframe shader with depth testing enabled.
class LightWireframeRenderer {
 public:
  // video must outlive this renderer.
  explicit LightWireframeRenderer(abstract::VideoDevice* video);
  ~LightWireframeRenderer();

  LightWireframeRenderer(const LightWireframeRenderer&)            = delete;
  LightWireframeRenderer& operator=(const LightWireframeRenderer&) = delete;

  // Draws all non-global lights in `objects` into `fbo`.
  // fbo must combine the scene colour RT with the GBuffer depth so depth
  // testing correctly occludes wireframes behind opaque geometry.
  // `selected` may be nullptr; the selected light is highlighted in blue.
  void Render(const std::vector<game::GameObject*>& objects,
              const game::GameObject* selected,
              abstract::RenderTargetGroup* fbo);

 private:
  // Appends a line segment p0→p1 with `color` to vertices_.
  void PushSegment(const core::Vec3f& p0, const core::Vec3f& p1,
                   const core::Color& color);

  // Appends a 32-segment circle approximation to vertices_.
  void PushCircle(const core::Vec3f& center,
                  const core::Vec3f& ax, const core::Vec3f& ay,
                  float radius, const core::Color& color);

  // Populates vertices_ with all light wireframe line segments.
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
