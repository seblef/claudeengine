#include "ui/UISprite.h"

namespace ui {

UISprite::UISprite(abstract::Texture* texture, const core::Rect& rect)
    : texture_(texture), rect_(rect), tint_(1.f, 1.f, 1.f, 1.f) {}

UISprite::UISprite(abstract::Texture* texture, float x, float y, float w,
                   float h)
    : UISprite(texture, core::Rect(x, y, w, h)) {}

void UISprite::SetPosition(float x, float y) { rect_.SetPosition(x, y); }

void UISprite::SetSize(float w, float h) { rect_.SetSize(w, h); }

void UISprite::SetTint(float r, float g, float b, float a) {
  tint_ = {r, g, b, a};
}

}  // namespace ui
