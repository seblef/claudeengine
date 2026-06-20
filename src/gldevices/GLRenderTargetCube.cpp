#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLRenderTargetCube.h"

#include <loguru.hpp>

namespace gldevices {

GLRenderTargetCube::GLRenderTargetCube(int size)
    : abstract::RenderTargetCube(size) {
  glGenTextures(1, &tex_id_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id_);

  for (int face = 0; face < 6; ++face) {
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0,
                 GL_DEPTH_COMPONENT32F, size, size, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  }

  // LINEAR filter + shadow compare mode enable samplerCubeShadow hardware PCF.
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE,
                  GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  LOG_F(5, "GLRenderTargetCube: created cube %dx%d tex=%u", size, size, tex_id_);
}

GLRenderTargetCube::~GLRenderTargetCube() {
  if (tex_id_) glDeleteTextures(1, &tex_id_);
}

void GLRenderTargetCube::BindAsSampler(int slot) {
  glActiveTexture(GL_TEXTURE0 + slot);
  glBindTexture(GL_TEXTURE_CUBE_MAP, tex_id_);
}

void GLRenderTargetCube::EnableCompareMode(bool enable) {
  glTextureParameteri(tex_id_, GL_TEXTURE_COMPARE_MODE,
                      enable ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
}

void GLRenderTargetCube::BindFaceAsOutput(int face_idx) {
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx,
                         tex_id_, 0);
}

}  // namespace gldevices
