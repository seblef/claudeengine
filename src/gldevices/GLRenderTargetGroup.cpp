#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLRenderTargetGroup.h"

#include <loguru.hpp>

namespace gldevices {

GLRenderTargetGroup::GLRenderTargetGroup(
    std::span<abstract::RenderTarget*> color_targets,
    abstract::RenderTarget* depth_stencil_target)
    : color_targets_(color_targets.begin(), color_targets.end()),
      depth_target_(depth_stencil_target) {
  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

  for (int i = 0; i < static_cast<int>(color_targets_.size()); ++i) {
    color_targets_[i]->BindAsOutput(i);
  }

  if (depth_target_) {
    depth_target_->BindAsOutput(0);  // index ignored for depth attachments
  }

  // Depth-only FBOs require explicit GL_NONE draw/read buffers to be complete.
  if (color_targets_.empty()) {
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
  }

  // A completely empty FBO is valid for deferred attachment (e.g. cube shadow
  // maps attach each face dynamically before rendering).
  if (color_targets_.empty() && !depth_target_) {
    LOG_F(5, "GLRenderTargetGroup: FBO %u created (deferred attachment)", fbo_);
  } else {
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      LOG_F(ERROR, "GLRenderTargetGroup: FBO incomplete (status=0x%x)", status);
    } else {
      LOG_F(5, "GLRenderTargetGroup: FBO %u created (%zu color + %s depth)",
            fbo_, color_targets_.size(), depth_target_ ? "1" : "no");
    }
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

GLRenderTargetGroup::~GLRenderTargetGroup() {
  if (fbo_) glDeleteFramebuffers(1, &fbo_);
}

void GLRenderTargetGroup::BindForWriting() {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo_);

  if (color_targets_.empty()) {
    glDrawBuffer(GL_NONE);
  } else {
    std::vector<GLenum> draw_buffers(color_targets_.size());
    for (int i = 0; i < static_cast<int>(color_targets_.size()); ++i) {
      draw_buffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(static_cast<GLsizei>(draw_buffers.size()), draw_buffers.data());
  }
}

void GLRenderTargetGroup::UnbindForWriting() {
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void GLRenderTargetGroup::BindForReading(int first_slot) {
  for (int i = 0; i < static_cast<int>(color_targets_.size()); ++i) {
    color_targets_[i]->BindAsSampler(first_slot + i);
  }
}

abstract::RenderTarget* GLRenderTargetGroup::GetColorTarget(int index) {
  return color_targets_[index];
}

abstract::RenderTarget* GLRenderTargetGroup::GetDepthTarget() {
  return depth_target_;
}

}  // namespace gldevices
