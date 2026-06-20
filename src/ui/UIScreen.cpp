#include "ui/UIScreen.h"

#include "ui/UIRenderer.h"

namespace ui {

UIScreen::~UIScreen() {
  if (visible_) Hide();
}

void UIScreen::Show() {
  if (visible_) return;
  UIRenderer& r = UIRenderer::Instance();
  for (const auto& s : sprites_) r.AddUISprite(s.get());
  for (const auto& t : texts_)   r.AddUIText(t.get());
  visible_ = true;
}

void UIScreen::Hide() {
  if (!visible_) return;
  UIRenderer& r = UIRenderer::Instance();
  for (const auto& s : sprites_) r.RemoveUISprite(s.get());
  for (const auto& t : texts_)   r.RemoveUIText(t.get());
  visible_ = false;
}

}  // namespace ui
