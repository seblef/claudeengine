#pragma once

#include <cstdint>

#include <GL/gl.h>

#include "abstract/RawTexture.h"

namespace gldevices {

// OpenGL R16 (unsigned-normalised) texture created from CPU memory.
// Used for terrain heightmaps; not registered in the resource registry.
class GLRawTexture : public abstract::RawTexture {
 public:
  GLRawTexture(int width, int height, const uint16_t* data);
  ~GLRawTexture() override;

  void Bind(int slot) override;

 private:
  // cppcheck-suppress unusedStructMember
  GLuint tex_id_ = 0;
};

}  // namespace gldevices
