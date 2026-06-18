#pragma once

#include <memory>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"

namespace renderer {

// Owns the three G-buffer render targets and the 2-colour-MRT FBO bundle.
//
// Render targets:
//   Albedo   (RGBA8)          — rgb=diffuse_texture × diffuse_color, a=spec_intensity
//   Normal   (RGBA16F)        — rgb=world-space normal encoded as N × 0.5 + 0.5,
//                               a=shininess/256
//   Depth    (DEPTH24STENCIL8)— depth for position reconstruction; stencil for
//                               light volume masking
//
// The depth RT is also borrowed by EmissiveFBO, which must be destroyed before
// this GBuffer is resized or destroyed.
class GBuffer {
 public:
  // Allocates all three RTs and the FBO at the given resolution.
  void Create(abstract::VideoDevice* video, int w, int h);

  // Destroys and recreates all RTs and the FBO at the new resolution.
  void Resize(abstract::VideoDevice* video, int w, int h);

  // Binds the FBO for writing (all 3 color draw buffers active).
  void BindForWriting();

  // Restores the default framebuffer as the draw target.
  void UnbindForWriting();

  // Binds each colour RT as a sampler starting at first_slot:
  //   first_slot+0 = albedo, first_slot+1 = normal.
  void BindForReading(int first_slot);

  // ---- Individual RT accessors (for lighting / debug passes) ---------------

  [[nodiscard]] abstract::RenderTarget* GetAlbedoRT() const;
  [[nodiscard]] abstract::RenderTarget* GetNormalRT() const;

  // Depth+stencil RT — also borrowed by EmissiveFBO for depth testing.
  [[nodiscard]] abstract::RenderTarget* GetDepthRT() const;

 private:
  // RTs declared before fbo_ so the FBO is destroyed first on cleanup.
  std::unique_ptr<abstract::RenderTarget>      albedo_rt_;
  std::unique_ptr<abstract::RenderTarget>      normal_rt_;
  std::unique_ptr<abstract::RenderTarget>      depth_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> fbo_;
};

}  // namespace renderer
