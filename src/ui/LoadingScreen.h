#pragma once

#include <cstdint>
#include <memory>

#include "abstract/RawTexture.h"
#include "ui/FontAtlas.h"
#include "ui/UIScreen.h"

namespace abstract { class VideoDevice; }

namespace ui {

// Full-screen overlay displayed during level transitions.
//
// Shows a solid black background, a centred "Loading..." label, and a progress
// bar whose width is driven by SetProgress(). Call Build() once before Show().
class LoadingScreen : public UIScreen {
 public:
  // Called once; creates the background, text, and progress bar elements.
  void Build(abstract::VideoDevice* video) override;

  // Updates the progress bar width to fraction * screen_width (fraction in [0, 1]).
  void SetProgress(float fraction);

 private:
  // 1×1 white RGBA tileable texture used as a solid-colour source for sprites.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<abstract::RawTexture> white_texture_;
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<FontAtlas>            font_atlas_;
  UISprite*                             progress_bar_ = nullptr;
  // cppcheck-suppress unusedStructMember
  float                                 screen_width_ = 0.f;
};

}  // namespace ui
