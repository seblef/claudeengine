// Must be defined before any GL/gl.h include to expose extension prototypes.
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLTileableTexture.h"

#include <loguru.hpp>

namespace gldevices {

GLTileableTexture::GLTileableTexture(int width, int height,
                                     const uint8_t* data)
    : width_(width), height_(height) {
  glGenTextures(1, &tex_id_);
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  LOG_F(5, "GLTileableTexture: uploaded %dx%d", width, height);
}

GLTileableTexture::~GLTileableTexture() {
  if (tex_id_) glDeleteTextures(1, &tex_id_);
}

void GLTileableTexture::Bind(int slot) {
  glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot));
  glBindTexture(GL_TEXTURE_2D, tex_id_);
}

void GLTileableTexture::UpdateRegion(int x, int y, int w, int h,
                                     const void* data) {
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h,
                  GL_RGBA, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace gldevices
