#include "ui/UISprite.h"

namespace ui {

UISprite::UISprite(abstract::Texture* texture, float x, float y, float w,
                   float h)
    : texture_(texture),
      rect_(x, y, w, h),
      tint_(1.f, 1.f, 1.f, 1.f) {}

void UISprite::SetPosition(float x, float y) {
  rect_.x = x;
  rect_.y = y;
}

void UISprite::SetSize(float w, float h) {
  rect_.z = w;
  rect_.w = h;
}

void UISprite::SetTint(float r, float g, float b, float a) {
  tint_ = {r, g, b, a};
}

}  // namespace ui
