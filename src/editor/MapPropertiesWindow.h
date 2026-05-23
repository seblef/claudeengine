#pragma once

#include <string>

#include "game/GameLightDesc.h"

namespace editor {

class EditorScene;

// Dual-mode window for map properties.
//
// Panel mode (constructed with a non-null EditorScene*): renders an
// always-visible properties panel. Light changes are applied immediately via
// EditorScene::SetGlobalLightDesc(); name changes via SetMapName(). Map size
// is read-only.
//
// Modal mode (constructed without a scene, or used standalone): renders an
// ImGui popup ("New Map##modal"). Returns true from RenderModal() exactly once
// when the user clicks "Create". Callers must call
// ImGui::OpenPopup("New Map##modal") before the first RenderModal() call each
// time they want to open it.
class MapPropertiesWindow {
 public:
  // Panel mode: scene must not be null.
  explicit MapPropertiesWindow(EditorScene* scene);

  // Renders the always-open properties panel (call inside an ImGui window).
  // Returns true if any field was edited this frame.
  bool RenderPanel();

  // Renders the "New Map##modal" popup. Returns true exactly once when the
  // user clicks "Create"; the entered values are then readable via the
  // GetNewMap*() accessors. Call ImGui::OpenPopup("New Map##modal") before
  // the first call each time you want to open the popup.
  bool RenderModal();

  [[nodiscard]] const std::string&          GetNewMapName()      const;
  [[nodiscard]] float                       GetNewMapSize()      const;
  // cppcheck-suppress returnByReference
  [[nodiscard]] const game::GameLightDesc&  GetNewMapLightDesc() const;

 private:
  // Renders all light-related fields into desc. Returns true if any field
  // was edited this frame.
  static bool RenderLightFields(game::GameLightDesc& desc, bool read_only_size);

  // cppcheck-suppress unusedStructMember
  EditorScene* scene_;  // not owned; null when used as a standalone modal

  // Scratch state for modal mode.
  // cppcheck-suppress unusedStructMember
  std::string         modal_name_  = "untitled";
  // cppcheck-suppress unusedStructMember
  float               modal_size_  = 120.f;
  // cppcheck-suppress unusedStructMember
  game::GameLightDesc modal_light_;
};

}  // namespace editor
