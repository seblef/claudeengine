#pragma once

#include <cstdint>

#include <GL/gl.h>

#include "abstract/RawTexture.h"

namespace gldevices {

// RGBA8 tileable texture with GL_REPEAT wrapping and mipmap filtering.
// Intended for procedural textures (water normal maps, foam) that tile
// seamlessly across the water surface.  Mipmaps are generated automatically
// at construction time.
class GLTileableTexture : public abstract::RawTexture {
 public:
  GLTileableTexture(int width, int height, const uint8_t* data);
  ~GLTileableTexture() override;

  void Bind(int slot) override;

  // Uploads a rectangular sub-region and regenerates mipmaps.
  void UpdateRegion(int x, int y, int w, int h, const void* data) override;

 private:
  // cppcheck-suppress unusedStructMember
  GLuint tex_id_ = 0;
  // cppcheck-suppress unusedStructMember
  int width_     = 0;
  // cppcheck-suppress unusedStructMember
  int height_    = 0;
};

}  // namespace gldevices
