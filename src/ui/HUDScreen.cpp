#include "ui/HUDScreen.h"

#include <string>

#include "abstract/Texture.h"
#include "abstract/VideoDevice.h"
#include "core/Config.h"
#include "ui/UISprite.h"
#include "ui/UIText.h"

namespace ui {

HUDScreen::~HUDScreen() {
  if (crosshair_texture_) {
    crosshair_texture_->Release();
    crosshair_texture_ = nullptr;
  }
}

void HUDScreen::Build(abstract::VideoDevice* video) {
  const float w = static_cast<float>(video->GetWidth());
  const float h = static_cast<float>(video->GetHeight());

  // ---- Crosshair sprite (centre of screen, 32×32 px) -----------------------
  crosshair_texture_ = video->CreateTexture("ui/crosshair.png");
  if (crosshair_texture_) {
    constexpr float kSize = 32.f;
    sprites_.push_back(std::make_unique<UISprite>(
        crosshair_texture_,
        w * 0.5f - kSize * 0.5f, h * 0.5f - kSize * 0.5f,
        kSize, kSize));
    sprites_.back()->SetLayer(1);
  }

  // ---- Font atlas -----------------------------------------------------------
  font_atlas_ = std::make_unique<FontAtlas>();
  const std::string font_path =
      (core::Config::GetDataFolder() / "fonts/fa-solid-900.ttf").string();
  font_atlas_->Load(video, font_path.c_str(), 24);

  // ---- Health label (bottom-left) ------------------------------------------
  constexpr float kMargin = 20.f;
  texts_.push_back(std::make_unique<UIText>(
      font_atlas_.get(), "HP: 100",
      kMargin, h - kMargin - static_cast<float>(font_atlas_->GetLineHeight()),
      1.f));
  texts_.back()->SetLayer(1);
  health_text_ = texts_.back().get();

  // ---- Ammo label (bottom-right) -------------------------------------------
  constexpr float kAmmoWidth = 120.f;
  texts_.push_back(std::make_unique<UIText>(
      font_atlas_.get(), "AMMO: 30",
      w - kAmmoWidth, h - kMargin - static_cast<float>(font_atlas_->GetLineHeight()),
      1.f));
  texts_.back()->SetLayer(1);
  ammo_text_ = texts_.back().get();
}

void HUDScreen::SetHealth(int health) {
  if (!health_text_) return;
  health_text_->SetText(("HP: " + std::to_string(health)).c_str());
}

void HUDScreen::SetAmmo(int ammo) {
  if (!ammo_text_) return;
  ammo_text_->SetText(("AMMO: " + std::to_string(ammo)).c_str());
}

}  // namespace ui
