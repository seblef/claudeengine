#pragma once

#include <memory>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"
#include "core/Mat4f.h"

namespace renderer {

// Owns the GPU resources for one shadow map: a kDepth32F render target and a
// depth-only FBO (no color attachments).
//
// SetLightVP() is called each frame by ShadowRenderer before the shadow pass.
// GetFBO() is bound for writing during the shadow pass; GetDepthRT() is later
// bound as a sampler2DShadow in the lighting shaders.
class ShadowMap {
 public:
  ShadowMap(abstract::VideoDevice* video, int resolution);

  ShadowMap(const ShadowMap&)            = delete;
  ShadowMap& operator=(const ShadowMap&) = delete;

  [[nodiscard]] abstract::RenderTarget*      GetDepthRT()  const { return depth_rt_.get(); }
  [[nodiscard]] abstract::RenderTargetGroup* GetFBO()      const { return fbo_.get(); }
  [[nodiscard]] const core::Mat4f&           GetLightVP()  const { return light_vp_; }
  [[nodiscard]] int                          GetResolution() const { return resolution_; }

  void SetLightVP(const core::Mat4f& vp) { light_vp_ = vp; }

 private:
  std::unique_ptr<abstract::RenderTarget>      depth_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> fbo_;
  core::Mat4f                                  light_vp_;
  int                                          resolution_;
};

}  // namespace renderer
