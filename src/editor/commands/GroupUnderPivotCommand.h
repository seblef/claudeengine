#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "editor/EditorCommand.h"

namespace game {
class GamePivot;
class GameObject;
}  // namespace game

namespace editor {

class EditorScene;

// Undoable command that groups a selection of objects under a new GamePivot.
//
// Execute: creates a pivot at the AABB centre of the selection and reparents
//          each object under it.
// Undo:    reparents each object back to the pivot's parent, then removes the
//          pivot from the scene.
// Redo:    re-places the same pivot and reparents the objects back under it.
class GroupUnderPivotCommand : public EditorCommand {
 public:
  GroupUnderPivotCommand(EditorScene*                     scene,
                         std::string                      pivot_name,
                         std::vector<game::GameObject*>   children);

  void Execute()          override;
  void Undo()             override;
  void Redo()             override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  EditorScene*                   scene_;
  // cppcheck-suppress unusedStructMember
  std::string                    pivot_name_;
  // cppcheck-suppress unusedStructMember
  std::vector<game::GameObject*> children_;

  // Ownership state: exactly one of these is non-null at any time.
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<game::GamePivot> owned_pivot_;   // non-null while undone
  // cppcheck-suppress unusedStructMember
  game::GamePivot*                 placed_pivot_ = nullptr;  // non-null while executed
};

}  // namespace editor
