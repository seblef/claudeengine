#pragma once

#include <memory>

#include "abstract/IndexBuffer.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Singleton.h"

namespace environment {

// Singleton procedural cloud-layer renderer.
//
// Draws a fullscreen pass after the sky background and before the geometry
// pass.  A view ray is projected onto a virtual cloud plane at a fixed
// altitude (800 m above the camera), and a 4-octave FBM noise field scrolled
// by the WindInfos constant buffer (slot 7) is sampled to derive cloud density.
//
// Blending: SRC_ALPHA / ONE_MINUS_SRC_ALPHA so clouds are composited over
// the sky HDR colour without adding light.
//
// Usage:
//   new CloudRenderer();
//   CloudRenderer::Instance().Build(video);
//   // each frame (after SkyRenderer, before geometry):
//   CloudRenderer::Instance().Render(world_time, cloud_density);
//   CloudRenderer::Instance().Reset();   // release GPU resources
//   CloudRenderer::Shutdown();           // delete instance
class CloudRenderer : public core::Singleton<CloudRenderer> {
 public:
  CloudRenderer() = default;
  ~CloudRenderer() = default;

  CloudRenderer(const CloudRenderer&)            = delete;
  CloudRenderer& operator=(const CloudRenderer&) = delete;
  CloudRenderer(CloudRenderer&&)                 = delete;
  CloudRenderer& operator=(CloudRenderer&&)      = delete;

  // Loads the cloud shader and reuses a fullscreen quad (same geometry as
  // SkyRenderer).  video must outlive this renderer.
  void Build(abstract::VideoDevice* video);

  // Draws the cloud layer into the currently bound HDR render target.
  // world_time : time of day in hours [0, 24) — forwarded as wind_time context.
  // cloud_density : coverage fraction in [0, 1].
  //                 0 = clear sky, 0.5 = partly cloudy, 0.9 = overcast.
  void Render(float world_time, float cloud_density);

  // Releases the shader and all GPU buffers.
  void Reset();

  [[nodiscard]] bool IsReady() const { return shader_ != nullptr; }

 private:
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                  video_   = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                       shader_  = nullptr;
  std::unique_ptr<abstract::VertexBuffer> quad_vb_;
  std::unique_ptr<abstract::IndexBuffer>  quad_ib_;
};

}  // namespace environment
