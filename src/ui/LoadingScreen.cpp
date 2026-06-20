#include "ui/LoadingScreen.h"

#include <algorithm>
#include <string>

#include "abstract/VideoDevice.h"
#include "core/Config.h"
#include "ui/UISprite.h"
#include "ui/UIText.h"

namespace ui {

void LoadingScreen::Build(abstract::VideoDevice* video) {
  const float w = static_cast<float>(video->GetWidth());
  const float h = static_cast<float>(video->GetHeight());
  screen_width_ = w;

  // ---- Shared 1×1 white texture for solid-colour quads ----------------------
  const uint8_t white[4] = {255, 255, 255, 255};
  white_texture_ = video->CreateTileableTexture(1, 1, white);

  // ---- Black full-screen background (layer 0) --------------------------------
  sprites_.push_back(std::make_unique<UISprite>(
      white_texture_.get(), 0.f, 0.f, w, h));
  sprites_.back()->SetTint(0.f, 0.f, 0.f, 1.f);
  sprites_.back()->SetLayer(0);

  // ---- Font atlas ------------------------------------------------------------
  font_atlas_ = std::make_unique<FontAtlas>();
  const std::string font_path =
      (core::Config::GetDataFolder() / "fonts/fa-solid-900.ttf").string();
  font_atlas_->Load(video, font_path.c_str(), 28);

  // ---- "Loading…" label (centred) -------------------------------------------
  constexpr float kTextEstWidth = 100.f;
  const float text_x = w * 0.5f - kTextEstWidth;
  const float text_y = h * 0.5f;
  texts_.push_back(std::make_unique<UIText>(
      font_atlas_.get(), "Loading...", text_x, text_y, 1.f));
  texts_.back()->SetColor(1.f, 1.f, 1.f, 1.f);
  texts_.back()->SetLayer(1);

  // ---- Progress bar (bottom, white, layer 1) --------------------------------
  constexpr float kBarH    = 8.f;
  constexpr float kBarY    = 0.85f;
  sprites_.push_back(std::make_unique<UISprite>(
      white_texture_.get(), 0.f, h * kBarY, 0.f, kBarH));
  sprites_.back()->SetTint(1.f, 1.f, 1.f, 1.f);
  sprites_.back()->SetLayer(1);
  progress_bar_ = sprites_.back().get();
}

void LoadingScreen::SetProgress(float fraction) {
  if (!progress_bar_) return;
  fraction = std::clamp(fraction, 0.f, 1.f);
  progress_bar_->SetSize(screen_width_ * fraction, 8.f);
}

}  // namespace ui
