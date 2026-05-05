#pragma once

#include <GL/gl.h>

#include "abstract/Texture.h"

namespace gldevices {

// OpenGL texture loaded from a file under data/textures/.
// Supports JPEG and PNG via stb_image; TIFF via libtiff when HAVE_LIBTIFF is defined.
class GLTexture : public abstract::Texture {
 public:
  GLTexture(const std::string& name, abstract::BufferUsage usage);
  ~GLTexture() override;

  void Bind(int slot = 0) override;

 private:
  GLuint tex_id_ = 0;
};

}  // namespace gldevices
