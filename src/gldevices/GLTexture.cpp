// Must be defined before any GL/gl.h include to expose extension prototypes.
#define GL_GLEXT_PROTOTYPES

#include "gldevices/GLTexture.h"

#include "core/Config.h"

#include <filesystem>

#include <loguru.hpp>
#include <stb_image.h>

#ifdef HAVE_LIBTIFF
#include <tiffio.h>
#endif

// GLI uses GLM experimental extensions and has a benign enum-conversion warning.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#define GLM_ENABLE_EXPERIMENTAL
#include <gli/gli.hpp>
#pragma GCC diagnostic pop

namespace gldevices {

GLTexture::GLTexture(const std::string& name, abstract::BufferUsage usage)
    : abstract::Texture(name, 0, 0, abstract::TextureFormat::kA8R8G8B8, usage) {
  const auto path = core::Config::GetDataFolder() / "textures" / name;
  const std::string ext = path.extension().string();

  // -------------------------------------------------------------------------
  // Compressed path: DDS / KTX via GLI
  // -------------------------------------------------------------------------
  if (ext == ".dds" || ext == ".ktx" || ext == ".ktx2") {
    if (!std::filesystem::exists(path)) {
      LOG_F(ERROR, "GLTexture '%s': file not found: '%s'",
            name.c_str(), path.string().c_str());
      return;
    }
    gli::texture raw = gli::load(path.string());
    if (raw.empty()) {
      LOG_F(ERROR, "GLTexture '%s': GLI failed to parse '%s' (invalid/unsupported format)",
            name.c_str(), path.string().c_str());
      return;
    }
    if (raw.target() != gli::TARGET_2D) {
      LOG_F(ERROR, "GLTexture '%s': '%s' is not a 2D texture (target=%d)",
            name.c_str(), path.string().c_str(), static_cast<int>(raw.target()));
      return;
    }

    gli::texture2d tex(raw);
    gli::gl GL(gli::gl::PROFILE_GL33);
    gli::gl::format const fmt = GL.translate(tex.format(), tex.swizzles());

    width_  = tex.extent().x;
    height_ = tex.extent().y;

    glGenTextures(1, &tex_id_);
    glBindTexture(GL_TEXTURE_2D, tex_id_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,
                    static_cast<GLint>(tex.levels() - 1));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    tex.levels() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R,
                    static_cast<GLint>(fmt.Swizzles[0]));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G,
                    static_cast<GLint>(fmt.Swizzles[1]));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B,
                    static_cast<GLint>(fmt.Swizzles[2]));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A,
                    static_cast<GLint>(fmt.Swizzles[3]));

    for (std::size_t level = 0; level < tex.levels(); ++level) {
      gli::extent2d const mip_ext = tex.extent(level);
      glCompressedTexImage2D(GL_TEXTURE_2D,
                             static_cast<GLint>(level),
                             fmt.Internal,
                             mip_ext.x, mip_ext.y, 0,
                             static_cast<GLsizei>(tex.size(level)),
                             tex.data(0, 0, level));
      GLenum gl_err = glGetError();
      if (gl_err != GL_NO_ERROR) {
        LOG_F(ERROR, "GLTexture '%s': glCompressedTexImage2D failed at mip %zu "
              "(GL error 0x%x, internal format 0x%x, %dx%d)",
              name.c_str(), level, gl_err, fmt.Internal, mip_ext.x, mip_ext.y);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &tex_id_);
        tex_id_ = 0;
        return;
      }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    initialized_ = true;
    LOG_F(5, "GLTexture '%s': loaded %dx%d (compressed, %zu mips)",
          name.c_str(), width_, height_, tex.levels());
    return;
  }

  // -------------------------------------------------------------------------
  // Uncompressed path: PNG / JPEG via stb_image; TIFF via libtiff
  // -------------------------------------------------------------------------
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
    if (!std::filesystem::exists(path)) {
      LOG_F(ERROR, "GLTexture '%s': file not found: '%s'",
            name.c_str(), path.string().c_str());
    } else {
      LOG_F(ERROR, "GLTexture '%s': failed to decode '%s' (unsupported format or corrupt file)",
            name.c_str(), path.string().c_str());
    }
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
  LOG_F(5, "GLTexture '%s': loaded %dx%d", name.c_str(), w, h);
}

GLTexture::~GLTexture() {
  if (tex_id_) glDeleteTextures(1, &tex_id_);
}

void GLTexture::Bind(int slot) {
  glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot));
  glBindTexture(GL_TEXTURE_2D, tex_id_);
}

}  // namespace gldevices
