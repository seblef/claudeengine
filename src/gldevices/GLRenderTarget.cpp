#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLRenderTarget.h"

#include <loguru.hpp>

namespace gldevices {

namespace {

struct GLFormatDesc {
  GLenum internal_format;
  GLenum base_format;
  GLenum type;
};

GLFormatDesc ToGLFormat(abstract::TextureFormat format) {
  switch (format) {
    case abstract::TextureFormat::kRGBA8:
      return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case abstract::TextureFormat::kRGBA16F:
      return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
    case abstract::TextureFormat::kG11R11B10F:
      return {GL_R11F_G11F_B10F, GL_RGB, GL_FLOAT};
    case abstract::TextureFormat::kDepth24Stencil8:
      return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
    case abstract::TextureFormat::kDepth32F:
      return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
    default:
      LOG_F(ERROR, "GLRenderTarget: unsupported TextureFormat %d",
            static_cast<int>(format));
      return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
  }
}

}  // namespace

GLRenderTarget::GLRenderTarget(int width, int height,
                               abstract::TextureFormat format)
    : abstract::RenderTarget(width, height, format) {
  const GLFormatDesc desc = ToGLFormat(format);
  glGenTextures(1, &tex_id_);
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glTexImage2D(GL_TEXTURE_2D, 0, desc.internal_format, width, height, 0,
               desc.base_format, desc.type, nullptr);
  if (format == abstract::TextureFormat::kDepth32F) {
    // LINEAR filter + shadow compare mode enable sampler2DShadow hardware PCF.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
                    GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    // Border depth 1.0: UV outside [0,1] → comparison returns "lit" (no shadow).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr float kBorderDepth[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, kBorderDepth);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  LOG_F(INFO, "GLRenderTarget: created %dx%d format=%d tex=%u",
        width, height, static_cast<int>(format), tex_id_);
}

GLRenderTarget::~GLRenderTarget() {
  if (tex_id_) glDeleteTextures(1, &tex_id_);
}

void GLRenderTarget::BindAsSampler(int slot) {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_2D, tex_id_);
}

void GLRenderTarget::BindAsOutput(int index) {
  if (format_ == abstract::TextureFormat::kDepth24Stencil8) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, tex_id_, 0);
  } else if (format_ == abstract::TextureFormat::kDepth32F) {
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, tex_id_, 0);
  } else {
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(index),
                           GL_TEXTURE_2D, tex_id_, 0);
  }
}

}  // namespace gldevices
