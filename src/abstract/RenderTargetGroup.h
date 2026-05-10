#pragma once

#include "abstract/RenderTarget.h"

namespace abstract {

// Bundle of render targets forming a single FBO (framebuffer object).
//
// Holds N color RenderTargets and one optional depth+stencil RenderTarget.
// The group does not own the targets; the caller must keep them alive for the
// duration of the group's lifetime.
//
// Lifecycle: created via VideoDevice::CreateRenderTargetGroup(); owned by the
// caller via unique_ptr.
class RenderTargetGroup {
 public:
  virtual ~RenderTargetGroup() = default;

  RenderTargetGroup(const RenderTargetGroup&)            = delete;
  RenderTargetGroup& operator=(const RenderTargetGroup&) = delete;

  // Binds the FBO for writing.  All color attachments become active draw buffers.
  virtual void BindForWriting() = 0;

  // Restores the default framebuffer (FBO 0) as the draw target.
  virtual void UnbindForWriting() = 0;

  // Binds each color target as a texture sampler, starting at first_slot.
  // Color target 0 → slot first_slot, color target 1 → slot first_slot+1, etc.
  virtual void BindForReading(int first_slot) = 0;

  // Returns the color render target at the given index.
  [[nodiscard]] virtual RenderTarget* GetColorTarget(int index) = 0;

  // Returns the depth+stencil render target, or nullptr if none was provided.
  [[nodiscard]] virtual RenderTarget* GetDepthTarget() = 0;

 protected:
  RenderTargetGroup() = default;
};

}  // namespace abstract
