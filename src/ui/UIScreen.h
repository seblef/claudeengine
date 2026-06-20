#pragma once

#include <memory>
#include <vector>

#include "ui/UISprite.h"
#include "ui/UIText.h"

namespace abstract { class VideoDevice; }

namespace ui {

// Abstract composable overlay layer — groups UISprites and UITexts into a
// named, switchable screen (HUD, loading, menus, …).
//
// Subclasses allocate their elements in Build() and expose typed setters for
// runtime updates. Only one or more screens should be visible at a time;
// calling Show()/Hide() registers/unregisters the elements with UIRenderer so
// invisible screens incur zero render cost.
class UIScreen {
 public:
  virtual ~UIScreen();

  // Called once after construction. Subclasses create sprites and texts here;
  // video is available for texture and atlas loading.
  virtual void Build(abstract::VideoDevice* video) = 0;

  // Registers all owned sprites and texts with UIRenderer.
  // No-op if the screen is already visible.
  void Show();

  // Unregisters all owned sprites and texts from UIRenderer.
  // No-op if the screen is already hidden.
  void Hide();

  [[nodiscard]] bool IsVisible() const { return visible_; }

 protected:
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<UISprite>> sprites_;
  // cppcheck-suppress unusedStructMember
  std::vector<std::unique_ptr<UIText>>   texts_;

 private:
  // cppcheck-suppress unusedStructMember
  bool visible_ = false;
};

}  // namespace ui
