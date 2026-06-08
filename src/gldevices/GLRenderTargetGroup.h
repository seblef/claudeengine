#pragma once

#include <span>
#include <vector>

#include <GL/gl.h>

#include "abstract/RenderTarget.h"
#include "abstract/RenderTargetGroup.h"

namespace gldevices {

// OpenGL framebuffer object (FBO) bundle.
//
// Wraps a GLuint FBO built from N color RenderTargets and one optional
// depth+stencil RenderTarget.  All targets are attached during construction;
// FBO completeness is verified and logged.  The group does NOT own the
// targets — they must outlive this object.
class GLRenderTargetGroup : public abstract::RenderTargetGroup {
 public:
  GLRenderTargetGroup(std::span<abstract::RenderTarget*> color_targets,
                      abstract::RenderTarget* depth_stencil_target);
  ~GLRenderTargetGroup() override;

  // Binds the FBO for writing and activates all color draw buffers.
  void BindForWriting() override;

  // Restores the default framebuffer (FBO 0).
  void UnbindForWriting() override;

  // Binds each color target as a sampler starting at first_slot.
  void BindForReading(int first_slot) override;

  [[nodiscard]] abstract::RenderTarget* GetColorTarget(int index) override;
  [[nodiscard]] abstract::RenderTarget* GetDepthTarget() override;

  // Returns the underlying GL FBO name for backend-side pixel reads.
  [[nodiscard]] GLuint GetFboId() const { return fbo_; }

 private:
  GLuint fbo_ = 0;
  // cppcheck-suppress unusedStructMember
  std::vector<abstract::RenderTarget*> color_targets_;
  // cppcheck-suppress unusedStructMember
  abstract::RenderTarget*              depth_target_ = nullptr;
};

}  // namespace gldevices
