#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "ui/FontAtlas.h"

#include <fstream>
#include <vector>

#include <loguru.hpp>

#include "abstract/VideoDevice.h"

namespace ui {

namespace {

// Tries to bake glyphs into an atlas of the given size.
// Returns true and fills pixels/chars on success; false if glyphs do not fit.
bool TryBake(const unsigned char* ttf_data, float pixel_height,
             int atlas_w, int atlas_h,
             std::vector<uint8_t>& pixels,
             stbtt_bakedchar (&chars)[FontAtlas::kNumGlyphs]) {
  pixels.assign(static_cast<std::size_t>(atlas_w) * atlas_h, 0);
  const int result = stbtt_BakeFontBitmap(
      ttf_data, 0, pixel_height,
      pixels.data(), atlas_w, atlas_h,
      FontAtlas::kFirstChar, FontAtlas::kNumGlyphs,
      chars);
  // stbtt_BakeFontBitmap returns the index of the first unused row (positive)
  // or a negative value if not all glyphs fit.
  return result > 0;
}

}  // namespace

bool FontAtlas::Load(abstract::VideoDevice* video, const char* path,
                     int pixel_height) {
  Destroy();

  // ---- Load TTF file -------------------------------------------------------
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f.is_open()) {
    LOG_F(ERROR, "FontAtlas::Load: cannot open '%s'", path);
    return false;
  }
  const auto file_size = static_cast<std::size_t>(f.tellg());
  f.seekg(0);
  std::vector<unsigned char> ttf_data(file_size);
  f.read(reinterpret_cast<char*>(ttf_data.data()),
         static_cast<std::streamsize>(file_size));
  if (!f) {
    LOG_F(ERROR, "FontAtlas::Load: read error on '%s'", path);
    return false;
  }

  // ---- Bake atlas ----------------------------------------------------------
  stbtt_bakedchar chars[kNumGlyphs];
  std::vector<uint8_t> pixels;
  int atlas_w = 512, atlas_h = 512;

  if (!TryBake(ttf_data.data(), static_cast<float>(pixel_height),
               atlas_w, atlas_h, pixels, chars)) {
    atlas_w = atlas_h = 1024;
    if (!TryBake(ttf_data.data(), static_cast<float>(pixel_height),
                 atlas_w, atlas_h, pixels, chars)) {
      LOG_F(ERROR, "FontAtlas::Load: glyphs do not fit in 1024x1024 atlas "
            "(path='%s', pixel_height=%d)", path, pixel_height);
      return false;
    }
  }

  // ---- Upload texture ------------------------------------------------------
  texture_ = video->CreateAtlasTexture(atlas_w, atlas_h, pixels.data());
  if (!texture_) {
    LOG_F(ERROR, "FontAtlas::Load: CreateAtlasTexture failed");
    return false;
  }

  // ---- Extract glyph metrics -----------------------------------------------
  const float fw = static_cast<float>(atlas_w);
  const float fh = static_cast<float>(atlas_h);

  for (int i = 0; i < kNumGlyphs; ++i) {
    const stbtt_bakedchar& bc = chars[i];
    GlyphInfo& g = glyphs_[i];
    g.uv_x0    = bc.x0 / fw;
    g.uv_y0    = bc.y0 / fh;
    g.uv_x1    = bc.x1 / fw;
    g.uv_y1    = bc.y1 / fh;
    g.x_bearing = static_cast<int>(bc.xoff);
    g.y_bearing = static_cast<int>(bc.yoff);
    g.advance   = static_cast<int>(bc.xadvance);
    g.width     = bc.x1 - bc.x0;
    g.height    = bc.y1 - bc.y0;
  }

  // ---- Line height ---------------------------------------------------------
  stbtt_fontinfo info;
  stbtt_InitFont(&info, ttf_data.data(),
                 stbtt_GetFontOffsetForIndex(ttf_data.data(), 0));
  const float scale = stbtt_ScaleForPixelHeight(&info, static_cast<float>(pixel_height));
  int ascent = 0, descent = 0, line_gap = 0;
  stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
  line_height_ = static_cast<int>((ascent - descent + line_gap) * scale);

  loaded_ = true;
  LOG_F(INFO, "FontAtlas::Load: '%s' baked at %dpx into %dx%d atlas",
        path, pixel_height, atlas_w, atlas_h);
  return true;
}

void FontAtlas::Destroy() {
  texture_.reset();
  line_height_ = 0;
  loaded_ = false;
}

const GlyphInfo* FontAtlas::GetGlyph(char c) const {
  if (!loaded_) return nullptr;
  const int idx = static_cast<unsigned char>(c) - kFirstChar;
  if (idx < 0 || idx >= kNumGlyphs) return nullptr;
  return &glyphs_[idx];
}

abstract::RawTexture* FontAtlas::GetTexture() const {
  return texture_.get();
}

}  // namespace ui
