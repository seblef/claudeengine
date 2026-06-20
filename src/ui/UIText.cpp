#include "ui/UIText.h"

namespace ui {

UIText::UIText(FontAtlas* atlas, const char* text, float x, float y,
               float size)
    : atlas_(atlas),
      text_(text),
      position_(x, y),
      size_(size),
      color_(1.f, 1.f, 1.f, 1.f) {}

void UIText::SetText(const char* text) { text_ = text; }

void UIText::SetPosition(float x, float y) { position_ = {x, y}; }

void UIText::SetSize(float size) { size_ = size; }

void UIText::SetColor(float r, float g, float b, float a) {
  color_ = {r, g, b, a};
}

}  // namespace ui
