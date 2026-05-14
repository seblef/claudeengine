#pragma once

#include "abstract/Shader.h"
#include "core/Singleton.h"
#include "renderer/MeshInstance.h"
#include "renderer/ObjectRenderer.h"

namespace renderer {

// Singleton batch renderer for MeshInstance objects.
//
// Lifecycle: created by Renderer constructor; destroyed by Renderer destructor.
// PrepareRender() sorts by material (primary) then by mesh (secondary) so that
// the most expensive state change (texture bind) happens as rarely as possible.
//
// Two render methods share the same sorted instance list (PrepareRender must be
// called once before either):
//   Render()         — writes all meshes to the G-buffer (geometry/gbuffer shader).
//   RenderEmissive() — additively draws only emissive meshes into the HDR RT
//                      (geometry/emissive shader), called before EndRender().
class MeshRenderer : public core::Singleton<MeshRenderer>,
                     public ObjectRenderer<MeshInstance> {
 public:
  explicit MeshRenderer(abstract::VideoDevice* video);
  ~MeshRenderer() override;

  // Renders all enqueued instances into the G-buffer.
  void Render() override;

  // Additively renders only emissive instances into the HDR render target.
  // Must be called after Render() and before EndRender().
  void RenderEmissive() override;

  // Renders all depth-queued instances into the currently bound shadow FBO.
  // Sorts by geometry to minimise VAO switches; clears the depth queue.
  void RenderDepth() override;

  // Like RenderDepth() but uses the cube shadow depth shader, which writes
  // linear distance to the light (normalised by range) as gl_FragDepth.
  // Must be used for omni-light cube shadow passes so that the stored depth
  // matches the comparison in omni_light_ps.glsl.
  void RenderDepthCube();

 private:
  // cppcheck-suppress unusedStructMember
  abstract::Shader* emissive_shader_;
  // cppcheck-suppress unusedStructMember
  abstract::Shader* cube_depth_shader_;
};

}  // namespace renderer
