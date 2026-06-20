#pragma once

#include <memory>

#include "abstract/RawTexture.h"

namespace abstract {
class VideoDevice;
}

namespace ui {

// Per-glyph metrics produced by FontAtlas::Load().
//
// UV coordinates are normalised to [0, 1] in atlas space.
// x_bearing and y_bearing are the pixel offsets from the pen origin to the
// top-left of the glyph bitmap (y positive = downward, matching screen space).
// advance is the number of pixels to advance the pen after drawing this glyph.
struct GlyphInfo {
  // cppcheck-suppress unusedStructMember
  float uv_x0;      // atlas UV left edge [0, 1]
  // cppcheck-suppress unusedStructMember
  float uv_y0;      // atlas UV top edge [0, 1]
  // cppcheck-suppress unusedStructMember
  float uv_x1;      // atlas UV right edge [0, 1]
  // cppcheck-suppress unusedStructMember
  float uv_y1;      // atlas UV bottom edge [0, 1]
  // cppcheck-suppress unusedStructMember
  int   x_bearing;  // horizontal offset from pen origin to glyph left edge
  // cppcheck-suppress unusedStructMember
  int   y_bearing;  // vertical offset from baseline to glyph top edge
  // cppcheck-suppress unusedStructMember
  int   advance;    // horizontal pen advance (pixels)
  // cppcheck-suppress unusedStructMember
  int   width;      // glyph bitmap width (pixels)
  // cppcheck-suppress unusedStructMember
  int   height;     // glyph bitmap height (pixels)
};

// Rasterizes a TTF font file into a single-channel GPU texture (R8).
//
// Bakes the printable ASCII range (codepoints 32–126) at a given pixel height.
// The resulting atlas texture and per-glyph metrics are valid until Destroy()
// is called or the FontAtlas is destroyed.
class FontAtlas {
 public:
  FontAtlas()  = default;
  ~FontAtlas() { Destroy(); }

  FontAtlas(const FontAtlas&)            = delete;
  FontAtlas& operator=(const FontAtlas&) = delete;

  // Loads and rasterizes the TTF file at path at the requested pixel height.
  // Uploads the greyscale atlas as an R8 GPU texture via video.
  // Returns false and logs an error if the file cannot be opened or the glyphs
  // do not fit in a 1024x1024 atlas.
  bool Load(abstract::VideoDevice* video, const char* path, int pixel_height);

  // Releases the GPU texture and resets all glyph data.
  void Destroy();

  // Returns the GlyphInfo for the ASCII printable character c (32–126),
  // or nullptr if c is outside that range or Load() has not been called.
  [[nodiscard]] const GlyphInfo* GetGlyph(char c) const;

  // Returns the uploaded atlas texture, or nullptr before Load().
  [[nodiscard]] abstract::RawTexture* GetTexture() const;

  // Returns the recommended line height in pixels, or 0 before Load().
  [[nodiscard]] int GetLineHeight() const { return line_height_; }

 public:
  static constexpr int kFirstChar = 32;   // first baked ASCII codepoint
  static constexpr int kLastChar  = 126;  // last baked ASCII codepoint
  static constexpr int kNumGlyphs = kLastChar - kFirstChar + 1;

 private:
  std::unique_ptr<abstract::RawTexture> texture_;
  // cppcheck-suppress unusedStructMember
  GlyphInfo glyphs_[kNumGlyphs] = {};
  int       line_height_         = 0;
  bool      loaded_              = false;
};

}  // namespace ui
