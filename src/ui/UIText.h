#pragma once

#include <string>

#include "core/Vec2f.h"
#include "core/Vec4f.h"
#include "ui/FontAtlas.h"

namespace ui {

// A string drawn in screen space using a FontAtlas glyph texture.
//
// UIText is a pure data type — it holds no GPU resources and issues no draw
// calls. UIRenderer consumes a list of UITexts each frame.
//
// Position is the baseline origin in pixels (origin = top-left of screen).
// Size is a scale factor relative to the atlas pixel_height; 1.0 = native size.
class UIText {
 public:
  // Constructs a text element with the given atlas, string, baseline position,
  // and scale. atlas is not owned by UIText and must outlive it.
  UIText(FontAtlas* atlas, const char* text, float x, float y, float size);

  // Replaces the stored string. Does not allocate per subsequent call once the
  // internal std::string capacity covers the new length.
  void SetText(const char* text);

  // Sets the baseline origin in pixels.
  void SetPosition(float x, float y);

  // Sets the scale factor relative to the atlas pixel_height.
  void SetSize(float size);

  // Sets the RGBA text color. Default: (1,1,1,1).
  void SetColor(float r, float g, float b, float a);

  // Sets the draw layer. Higher values render on top. Default: 0.
  void SetLayer(int layer) { layer_ = layer; }

  [[nodiscard]] FontAtlas*      GetAtlas()    const { return atlas_; }
  [[nodiscard]] const char*     GetText()     const { return text_.c_str(); }
  [[nodiscard]] core::Vec2f     GetPosition() const { return position_; }
  [[nodiscard]] float           GetSize()     const { return size_; }
  [[nodiscard]] core::Vec4f     GetColor()    const { return color_; }
  [[nodiscard]] int             GetLayer()    const { return layer_; }

 private:
  FontAtlas*   atlas_;
  std::string  text_;
  core::Vec2f  position_;
  float        size_;
  core::Vec4f  color_;
  int          layer_ = 0;
};

}  // namespace ui
