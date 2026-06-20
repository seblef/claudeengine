#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "editor/EditorCommand.h"

namespace game {
class GamePivot;
class GameObject;
}  // namespace game

namespace editor {

class EditorScene;

// Undoable command that creates a new GamePivot, adds it to the scene,
// and optionally reparents it under a given parent.
//
// Execute: creates the pivot, adds it to the scene, reparents under parent
//          (no-op when parent is nullptr — pivot is placed at root level).
// Undo:    reclaims the pivot from the scene.
// Redo:    re-adds the same pivot and restores its parent.
class AddChildPivotCommand : public EditorCommand {
 public:
  // parent == nullptr creates a root-level pivot.
  AddChildPivotCommand(EditorScene* scene,
                       std::string  name,
                       game::GameObject* parent);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  EditorScene*      scene_;
  // cppcheck-suppress unusedStructMember
  std::string       name_;
  // cppcheck-suppress unusedStructMember
  game::GameObject* parent_;

  // Ownership invariant: exactly one of owned_pivot_ / placed_pivot_ is valid.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<game::GamePivot> owned_pivot_;    // non-null while undone
  // cppcheck-suppress unusedStructMember
  game::GamePivot*                 placed_pivot_ = nullptr;  // non-null while executed
};

}  // namespace editor
