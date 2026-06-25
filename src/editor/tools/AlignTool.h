#pragma once

#include <functional>

#include "core/BBox3.h"
#include "core/Event.h"
#include "core/Vec3f.h"
#include "editor/tools/EditorToolBase.h"

#include <imgui.h>

namespace game { class GameObject; }

namespace editor {

class EditorCommandHistory;
class EditorScene;

// Editor tool that snaps the position of selected objects to a target object
// along any combination of axes.
//
// State machine:
//   kPickingTarget — viewport awaits a target click; hovering non-selected
//                    objects shows a yellow wireframe bbox.  Pressing Escape
//                    cancels and fires on_done_.
//   kShowingModal  — alignment-options modal is open.  Apply commits via a
//                    MultiTransformCommand (undoable); Cancel discards.
//                    Both paths fire on_done_.
//
// Options (source/target reference points, group mode) persist across
// activations — AlignTool lives for the editor session lifetime.
class AlignTool : public EditorToolBase {
 public:
  // Alignment options remembered across modal reopens.
  struct AlignOptions {
    // cppcheck-suppress unusedStructMember
    bool align[3]   = {true, false, false};  // per axis: align or ignore
    // cppcheck-suppress unusedStructMember
    int  src_ref[3] = {1, 1, 1};             // 0=min, 1=center, 2=max
    // cppcheck-suppress unusedStructMember
    int  tgt_ref[3] = {1, 1, 1};
    // cppcheck-suppress unusedStructMember
    bool group_mode = false;
  };

  void OnActivate(const EditorToolContext& ctx) override;
  void OnDeactivate() override;
  void OnEvent(const core::Event& event) override;
  void OnRender(const EditorToolContext& ctx,
                ImVec2 image_pos, ImVec2 image_size) override;
  bool IsCapturingMouse() const override;

  // Fired when the tool exits (Apply, Cancel, or Escape). Wire to restore
  // the previously active viewport tool in EditorWindow.
  void SetOnDone(std::function<void()> cb) { on_done_ = std::move(cb); }

 private:
  enum class State { kPickingTarget, kShowingModal };
  enum class ModalResult { kOpen, kApply, kCancel };

  // Returns the min/center/max value of bbox on the given axis (0=X,1=Y,2=Z).
  static float RefPoint(const core::BBox3& bbox, int axis, int ref);

  // Translates selected objects to align with target_ and pushes a command.
  void ApplyAlignment(const EditorToolContext& ctx);

  // Draws the modal each frame while it is open; returns the user action.
  ModalResult RenderModal();

  // Resets per-activation state and fires on_done_.
  void Done();

  State state_ = State::kPickingTarget;

  // Non-owning; valid between OnActivate() and OnDeactivate().
  // cppcheck-suppress unusedStructMember
  EditorScene*          scene_   = nullptr;
  // cppcheck-suppress unusedStructMember
  EditorCommandHistory* history_ = nullptr;

  // Hovered candidate (not selected) in kPickingTarget mode; not owned.
  // cppcheck-suppress unusedStructMember
  game::GameObject* hovered_ = nullptr;
  // Confirmed align target; set on click in kPickingTarget mode; not owned.
  // cppcheck-suppress unusedStructMember
  game::GameObject* target_  = nullptr;

  // Triggers ImGui::OpenPopup on the first kShowingModal frame.
  // cppcheck-suppress unusedStructMember
  bool open_popup_ = false;

  // Options persist across activations (session-scoped).
  // cppcheck-suppress unusedStructMember
  AlignOptions options_;

  // cppcheck-suppress unusedStructMember
  std::function<void()> on_done_;
};

}  // namespace editor
