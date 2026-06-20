#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "editor/EditorCommand.h"

namespace game {
class GamePivot;
class GameObject;
}  // namespace game

namespace editor {

class EditorScene;

// Undoable command that removes a GamePivot and promotes its children to the
// pivot's parent (or to root when the pivot has no parent).
//
// Execute: reparents the pivot's children to the pivot's parent, then removes
//          the pivot from the scene.
// Undo:    re-adds the pivot and reparents the children back under it.
class UngroupPivotCommand : public EditorCommand {
 public:
  UngroupPivotCommand(EditorScene* scene, game::GamePivot* pivot);

  void Execute()          override;
  void Undo()             override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  EditorScene*                   scene_;

  // Ownership state: exactly one of these is non-null at any time.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<game::GamePivot> owned_pivot_;   // non-null while executed
  // cppcheck-suppress unusedStructMember
  game::GamePivot*                 placed_pivot_ = nullptr;  // non-null while undone

  // Children that were reparented on Execute (captured at construction).
  // cppcheck-suppress unusedStructMember
  std::vector<game::GameObject*> children_;
  // Original parent of the pivot (may be nullptr for root-level pivots).
  // cppcheck-suppress unusedStructMember
  game::GameObject*              pivot_parent_ = nullptr;
};

}  // namespace editor
