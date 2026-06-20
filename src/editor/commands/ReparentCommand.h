#pragma once

#include <string_view>

#include "editor/EditorCommand.h"

namespace game {
class GameObject;
}  // namespace game

namespace editor {

// Undoable command that reparents a GameObject to a new parent.
//
// Preserves world transform: AddChild / RemoveChild already re-derive the
// local transform so the object stays at its current world position.
//
// Execute: obj->Reparent(new_parent)
// Undo:    obj->Reparent(old_parent)
class ReparentCommand : public EditorCommand {
 public:
  // new_parent == nullptr detaches obj from its current parent (moves to root).
  ReparentCommand(game::GameObject* obj, game::GameObject* new_parent);

  void Execute() override;
  void Undo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  game::GameObject* obj_;
  // cppcheck-suppress unusedStructMember
  game::GameObject* old_parent_;
  // cppcheck-suppress unusedStructMember
  game::GameObject* new_parent_;
};

}  // namespace editor
