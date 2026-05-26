#pragma once

#include <cstdint>

#include <GL/gl.h>

#include "abstract/RawTexture.h"

namespace gldevices {

// OpenGL RGBA8 texture for terrain normal maps.
// Supports incremental UpdateRegion() calls for sculpt-brush dirty tiles.
class GLNormalMapTexture : public abstract::RawTexture {
 public:
  GLNormalMapTexture(int width, int height, const uint8_t* data);
  ~GLNormalMapTexture() override;

  void Bind(int slot) override;
  void UpdateRegion(int x, int y, int w, int h, const void* data) override;

 private:
  // cppcheck-suppress unusedStructMember
  GLuint tex_id_ = 0;
};

}  // namespace gldevices
