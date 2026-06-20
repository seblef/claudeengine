#pragma once

#include "abstract/Texture.h"
#include "core/Rect.h"
#include "core/Vec4f.h"

namespace ui {

// A textured quad positioned in screen space (pixels, origin = top-left).
//
// UISprite is a pure data type — it holds no GPU resources and issues no draw
// calls. UIRenderer consumes a list of UISprites each frame to batch geometry.
class UISprite {
 public:
  // Constructs a sprite from a rect (position + size in pixels).
  // texture is not owned by UISprite and must outlive it.
  UISprite(abstract::Texture* texture, const core::Rect& rect);

  // Convenience constructor: position (x, y) and size (w, h) in pixels.
  UISprite(abstract::Texture* texture, float x, float y, float w, float h);

  // Sets the top-left corner position in pixels.
  void SetPosition(float x, float y);

  // Sets the width and height in pixels.
  void SetSize(float w, float h);

  // Sets the RGBA multiplicative tint applied at render time. Default: (1,1,1,1).
  void SetTint(float r, float g, float b, float a);

  [[nodiscard]] abstract::Texture*   GetTexture() const { return texture_; }
  [[nodiscard]] const core::Rect&    GetRect()    const { return rect_; }
  [[nodiscard]] core::Vec4f          GetTint()    const { return tint_; }

 private:
  abstract::Texture* texture_;
  core::Rect         rect_;
  core::Vec4f        tint_;
};

}  // namespace ui
