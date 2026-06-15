#pragma once

#include "core/Event.h"
#include "editor/tools/EditorToolContext.h"

#include <imgui.h>

namespace editor {

// Abstract base class for all editor interaction tools.
//
// Tools are activated and deactivated by EditorViewport via OnActivate() /
// OnDeactivate(). Each frame the viewport calls OnEvent() for platform events
// and OnRender() once the scene texture has been composited into the panel.
//
// Tools do not own any subsystem — they receive a non-owning EditorToolContext
// at activation time and must not cache it past OnDeactivate().
class EditorToolBase {
 public:
  virtual ~EditorToolBase() = default;

  // Called when this tool becomes the active tool.
  virtual void OnActivate(const EditorToolContext& ctx) {}

  // Called when another tool replaces this one.
  virtual void OnDeactivate() {}

  // Forwards a platform event (keyboard, mouse, window) to the tool.
  virtual void OnEvent(const core::Event& event) {}

  // Called inside the active ImGui viewport window after the scene is rendered.
  virtual void OnRender(const EditorToolContext& ctx,
                        ImVec2 image_pos, ImVec2 image_size) {}

  // Returns true if the tool is consuming LMB this frame, preventing the
  // viewport from also performing object picking.
  virtual bool IsCapturingMouse() const { return false; }
};

}  // namespace editor
