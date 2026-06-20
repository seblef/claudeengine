// Must be defined before any GL/gl.h include to expose extension prototypes.
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLAtlasTexture.h"

#include <loguru.hpp>

namespace gldevices {

GLAtlasTexture::GLAtlasTexture(int width, int height, const uint8_t* data) {
  glGenTextures(1, &tex_id_);
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  // GL_R8 internal format: 1 byte per texel, read as float [0,1] in shaders.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0,
               GL_RED, GL_UNSIGNED_BYTE, data);
  glBindTexture(GL_TEXTURE_2D, 0);
  LOG_F(5, "GLAtlasTexture: uploaded font atlas %dx%d", width, height);
}

GLAtlasTexture::~GLAtlasTexture() {
  if (tex_id_) glDeleteTextures(1, &tex_id_);
}

void GLAtlasTexture::Bind(int slot) {
  glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot));
  glBindTexture(GL_TEXTURE_2D, tex_id_);
}

void GLAtlasTexture::UpdateRegion(int x, int y, int w, int h,
                                   const void* data) {
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                  GL_RED, GL_UNSIGNED_BYTE, data);
  glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace gldevices
