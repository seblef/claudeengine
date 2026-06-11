// Must be defined before any GL/gl.h include to expose extension prototypes.
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLTexture.h"

#include "core/Config.h"

#include <loguru.hpp>
#include <stb_image.h>

#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif

namespace gldevices {

GLTexture::GLTexture(const std::string& name, abstract::BufferUsage usage)
    : abstract::Texture(name, 0, 0, abstract::TextureFormat::kA8R8G8B8, usage) {
  const auto path = core::Config::GetDataFolder() / "textures" / name;
  const std::string ext = path.extension().string();

  int w = 0, h = 0;
  unsigned char* pixels = nullptr;

  if (ext == ".tif" || ext == ".tiff") {
#ifdef HAVE_LIBTIFF
    TIFF* tif = TIFFOpen(path.string().c_str(), "r");
    if (tif) {
      uint32_t tw = 0, th = 0;
      TIFFGetField(tif, TIFFTAG_IMAGEWIDTH,  &tw);
      TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &th);
      w = static_cast<int>(tw);
      h = static_cast<int>(th);
      pixels = static_cast<unsigned char*>(_TIFFmalloc(w * h * 4));
      TIFFReadRGBAImageOriented(tif, tw, th,
          reinterpret_cast<uint32_t*>(pixels), ORIENTATION_TOPLEFT, 0);
      TIFFClose(tif);
    }
#else
    LOG_F(ERROR, "GLTexture '%s': TIFF support not compiled in", name.c_str());
    return;
#endif
  } else {
    stbi_set_flip_vertically_on_load(1);
    int channels = 0;
    pixels = stbi_load(path.string().c_str(), &w, &h, &channels, 4);
  }

  if (!pixels) {
    LOG_F(ERROR, "GLTexture '%s': failed to load '%s'",
          name.c_str(), path.string().c_str());
    return;
  }

  width_  = w;
  height_ = h;

  glGenTextures(1, &tex_id_);
  glBindTexture(GL_TEXTURE_2D, tex_id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);

  if (ext == ".tif" || ext == ".tiff") {
#ifdef HAVE_LIBTIFF
    _TIFFfree(pixels);
#endif
  } else {
    stbi_image_free(pixels);
  }

  initialized_ = true;
  LOG_F(INFO, "GLTexture '%s': loaded %dx%d", name.c_str(), w, h);
}

GLTexture::~GLTexture() {
  if (tex_id_) glDeleteTextures(1, &tex_id_);
}

void GLTexture::Bind(int slot) {
  glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot));
  glBindTexture(GL_TEXTURE_2D, tex_id_);
}

}  // namespace gldevices
