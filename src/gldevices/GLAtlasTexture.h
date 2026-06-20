#pragma once

#include <cstdint>

#include <GL/gl.h>

#include "abstract/RawTexture.h"

namespace gldevices {

// OpenGL R8 (single-channel, unsigned-normalised) texture for font atlases.
// Uses GL_CLAMP_TO_EDGE wrapping and GL_LINEAR filtering with no mipmaps.
// Not registered in the resource registry; callers own it via unique_ptr.
class GLAtlasTexture : public abstract::RawTexture {
 public:
  GLAtlasTexture(int width, int height, const uint8_t* data);
  ~GLAtlasTexture() override;

  void Bind(int slot) override;
  void UpdateRegion(int x, int y, int w, int h, const void* data) override;

 private:
  // cppcheck-suppress unusedStructMember
  GLuint tex_id_ = 0;
};

}  // namespace gldevices
