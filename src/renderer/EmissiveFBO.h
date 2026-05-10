#pragma once

#include <memory>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"
#include "abstract/VideoDevice.h"

namespace renderer {

// Owns the HDR accumulation render target and the emissive-pass FBO.
//
// The FBO uses the HDR RT as its color output and borrows the G-buffer
// depth+stencil RT (from GBuffer::GetDepthRT()) for depth testing during
// the emissive pass. The borrowed depth RT must outlive this object.
class EmissiveFBO {
 public:
  // Allocates the HDR RT (RGBA16F) and builds the FBO.
  // depth_rt is borrowed from GBuffer and must outlive this EmissiveFBO.
  void Create(abstract::VideoDevice* video, int w, int h,
              abstract::RenderTarget* depth_rt);

  // Destroys and recreates the HDR RT and FBO at the new resolution.
  // depth_rt must be the depth RT from the already-resized GBuffer.
  void Resize(abstract::VideoDevice* video, int w, int h,
              abstract::RenderTarget* depth_rt);

  // Binds the FBO for writing (HDR RT as color output).
  void BindForWriting();

  // Restores the default framebuffer as the draw target.
  void UnbindForWriting();

  // Returns the HDR accumulation render target.
  [[nodiscard]] abstract::RenderTarget* GetHDRRT() const;

 private:
  // hdr_rt_ declared before fbo_ so the FBO is destroyed first on cleanup.
  std::unique_ptr<abstract::RenderTarget>      hdr_rt_;
  std::unique_ptr<abstract::RenderTargetGroup> fbo_;
};

}  // namespace renderer
