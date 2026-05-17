#pragma once

#include <string_view>

#include "core/Mat4f.h"
#include "editor/EditorCommand.h"

namespace game {
class GameObject;
}  // namespace game

namespace editor {

// Command that records a single ImGuizmo-driven transform change (translate,
// rotate, or scale) on a GameObject, enabling undo/redo.
//
// Captures the world transform at drag-start (before_) and drag-end (after_).
// Intermediate per-frame mutations are NOT stored; only the net delta matters.
class TransformCommand : public EditorCommand {
 public:
  TransformCommand(game::GameObject* obj,
                   const core::Mat4f& before,
                   const core::Mat4f& after);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  game::GameObject* obj_;
  // cppcheck-suppress unusedStructMember
  core::Mat4f       before_;
  // cppcheck-suppress unusedStructMember
  core::Mat4f       after_;
};

}  // namespace editor
