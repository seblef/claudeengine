#pragma once

#include <memory>

#include "ui/FontAtlas.h"
#include "ui/UIScreen.h"

namespace abstract {
class Texture;
class VideoDevice;
}

namespace ui {

// In-game HUD overlay: crosshair, health label, ammo label.
//
// Call Build() once before Show(). SetHealth() / SetAmmo() may be called at
// any time (including while hidden) to update the displayed values.
class HUDScreen : public UIScreen {
 public:
  ~HUDScreen() override;

  // Creates the crosshair sprite and health/ammo text elements.
  // Loads "ui/crosshair.png" from the texture registry and bakes a font atlas
  // from "fonts/fa-solid-900.ttf".
  void Build(abstract::VideoDevice* video) override;

  // Updates the health label to "HP: <health>".
  void SetHealth(int health);

  // Updates the ammo label to "AMMO: <ammo>".
  void SetAmmo(int ammo);

 private:
  // cppcheck-suppress unusedStructMember
  abstract::Texture*          crosshair_texture_ = nullptr;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<FontAtlas>  font_atlas_;
  UIText*                     health_text_       = nullptr;
  UIText*                     ammo_text_         = nullptr;
};

}  // namespace ui
