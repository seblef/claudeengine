#pragma once

#include <memory>

#include "abstract/IndexBuffer.h"
#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/Shader.h"
#include "abstract/VertexBuffer.h"
#include "abstract/VideoDevice.h"
#include "core/Singleton.h"

namespace environment {

// Singleton renderer that bakes cloud density top-down into a world-space
// R16F render target each frame.
//
// The shadow texture covers a square area of ±coverage_radius metres around
// the camera (eye_pos.xz).  It is sampled by the GlobalLight deferred-lighting
// shader (global_light_ps.glsl, sampler binding 13) with a sun-direction offset
// so that cloud density is projected as soft transparent shadows on lit ground.
//
// Usage:
//   new CloudShadowRenderer();
//   CloudShadowRenderer::Instance().Build(video);
//   // each frame — before the lighting pass:
//   CloudShadowRenderer::Instance().Render(cloud_density);
//   // bind at sampler 13 before the GlobalLight draw, then unbind.
//   CloudShadowRenderer::Instance().Reset();
//   CloudShadowRenderer::Shutdown();
class CloudShadowRenderer : public core::Singleton<CloudShadowRenderer> {
 public:
  CloudShadowRenderer() = default;
  ~CloudShadowRenderer() = default;

  CloudShadowRenderer(const CloudShadowRenderer&)            = delete;
  CloudShadowRenderer& operator=(const CloudShadowRenderer&) = delete;
  CloudShadowRenderer(CloudShadowRenderer&&)                 = delete;
  CloudShadowRenderer& operator=(CloudShadowRenderer&&)      = delete;

  // Creates the 1024×1024 R16F shadow render target and loads the shader.
  // video must outlive this renderer.
  void Build(abstract::VideoDevice* video);

  // Renders the cloud density field top-down into the internal shadow texture.
  // cloud_density : coverage fraction in [0, 1], forwarded to the shader as the
  //                 threshold that matches the visual cloud coverage.
  void Render(float cloud_density);

  // Releases GPU resources.
  void Reset();

  // Returns the shadow texture for binding as a sampler in the lighting pass.
  // Returns nullptr before Build() or after Reset().
  [[nodiscard]] abstract::RenderTarget* GetShadowTexture() const {
    return shadow_rt_.get();
  }

  [[nodiscard]] bool IsReady() const { return shader_ != nullptr; }

  // Half-extent of the world area covered by the shadow texture (metres).
  // 1 texel = (2 × coverage_radius) / 1024 m.
  [[nodiscard]] float GetCoverageRadius()  const { return coverage_radius_; }
  [[nodiscard]] float GetShadowIntensity() const { return shadow_intensity_; }

  void SetCoverageRadius(float r)  { coverage_radius_  = r; }
  void SetShadowIntensity(float i) { shadow_intensity_  = i; }

 private:
  // cppcheck-suppress unusedStructMember
  abstract::VideoDevice*                       video_          = nullptr;
  // cppcheck-suppress unusedStructMember
  abstract::Shader*                            shader_         = nullptr;
  std::unique_ptr<abstract::RenderTarget>      shadow_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> shadow_fbo_;
  std::unique_ptr<abstract::VertexBuffer>      quad_vb_;
  std::unique_ptr<abstract::IndexBuffer>       quad_ib_;

  // Tunable parameters (art-side defaults).
  // cppcheck-suppress unusedStructMember
  float coverage_radius_  = 4000.f;  // 4 km → 1 texel ≈ 7.8 m at 1024²
  // cppcheck-suppress unusedStructMember
  float shadow_intensity_ = 0.6f;    // max shadow darkening [0, 1]
};

}  // namespace environment
