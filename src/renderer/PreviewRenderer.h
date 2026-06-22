#pragma once

#include <memory>
#include <span>

#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "abstract/VertexBuffer.h"
#include "core/Camera.h"
#include "renderer/EmissiveFBO.h"
#include "renderer/GBuffer.h"
#include "renderer/GeometryData.h"

namespace abstract { class Shader; }

namespace renderer {

class GlobalLight;
class MeshInstance;

// Isolated deferred renderer for offscreen previews.
//
// Owns its own GBuffer, EmissiveFBO, and scene constant buffer so that
// preview renders are independent of the main Renderer singleton's targets.
// Reuses the singleton MeshRenderer and LightRenderer for the draw calls but
// runs them against its own render targets.
//
// Intended usage: call Render() once per ImGui frame after
// renderer::Renderer::Update() has already completed, so the singleton
// MeshRenderer and LightRenderer queues are clean.
//
// After compositing, a world-space axes gizmo (X=red, Y=green, Z=blue,
// 1 unit each) is drawn on top of the output with depth testing disabled.
class PreviewRenderer {
 public:
  // Creates GBuffer, EmissiveFBO, scene CB, and loads the composite shader.
  PreviewRenderer(abstract::VideoDevice* video, int width, int height);
  ~PreviewRenderer();

  PreviewRenderer(const PreviewRenderer&)            = delete;
  PreviewRenderer& operator=(const PreviewRenderer&) = delete;

  // Renders mesh_instance lit by light into output_rtg, then overlays the
  // axes gizmo.
  // Binds the preview scene CB to UBO slot 2 so shaders see preview camera
  // matrices; rebinds the main Renderer's CB are restored at the next
  // Renderer::Update() call.
  void Render(float time, const core::Camera& camera,
              MeshInstance* mesh_instance,
              GlobalLight* light,
              abstract::RenderTargetGroup* output_rtg);

  // Multi-mesh variant: enqueues every instance in instances before the
  // geometry pass. nullptr entries are silently skipped.
  void Render(float time, const core::Camera& camera,
              std::span<MeshInstance* const> instances,
              GlobalLight* light,
              abstract::RenderTargetGroup* output_rtg);

 private:
  void FillSceneInfos(const core::Camera& camera, float time);

  // Draws the three world-space axis segments into output_rtg on top of the
  // composited scene (depth test disabled so axes are always visible).
  void DrawAxes(abstract::RenderTargetGroup* output_rtg);

  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice* video_;
  // cppcheck-suppress unusedStructMember
  int                    w_;
  // cppcheck-suppress unusedStructMember
  int                    h_;

  // cppcheck-suppress unusedStructMember
  GBuffer     gbuffer_;
  // cppcheck-suppress unusedStructMember
  EmissiveFBO emissive_fbo_;

  // Scene constant buffer (UBO slot 2) — preview camera matrices.
  std::unique_ptr<abstract::ConstantBuffer> scene_infos_cb_;

  // Composite pass — gamma-correct the HDR RT to the output RTG.
  // cppcheck-suppress unusedStructMember
  abstract::Shader*             composite_shader_;
  std::unique_ptr<GeometryData> composite_quad_;

  // 1×1 black RT bound to sampler 11 so the composite shader's u_bloom uniform
  // is always valid (bloom is never active in preview renders).
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RenderTarget> null_bloom_rt_;

  // Axes gizmo — three colored line segments drawn on top of the composited
  // scene with depth testing disabled.
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                        axes_shader_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::VertexBuffer>  axes_vb_;
};

}  // namespace renderer
