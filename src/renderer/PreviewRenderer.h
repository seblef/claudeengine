#pragma once

#include <memory>

#include "abstract/ConstantBuffer.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
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
class PreviewRenderer {
 public:
  // Creates GBuffer, EmissiveFBO, scene CB, and loads the composite shader.
  PreviewRenderer(abstract::VideoDevice* video, int width, int height);
  ~PreviewRenderer();

  PreviewRenderer(const PreviewRenderer&)            = delete;
  PreviewRenderer& operator=(const PreviewRenderer&) = delete;

  // Renders mesh_instance lit by light into output_rtg.
  // Binds the preview scene CB to UBO slot 2 so shaders see preview camera
  // matrices; rebinds the main Renderer's CB are restored at the next
  // Renderer::Update() call.
  void Render(float time, const core::Camera& camera,
              MeshInstance* mesh_instance,
              GlobalLight* light,
              abstract::RenderTargetGroup* output_rtg);

 private:
  void FillSceneInfos(const core::Camera& camera, float time);

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
};

}  // namespace renderer
