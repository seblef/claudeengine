#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "editor/EditorCommand.h"

namespace game {
class GameObject;
}  // namespace game

namespace editor {

class EditorScene;

// Undoable command that removes a GameObject and all its descendants from
// the scene.
//
// Execute: reclaims all dynamic nodes bottom-up (leaves first), preserving
//          no sibling side-effects.
// Undo:    re-adds all nodes top-down and restores the original parent links.
// Redo:    repeats the bottom-up reclaim.
class DeleteSubtreeCommand : public EditorCommand {
 public:
  DeleteSubtreeCommand(EditorScene* scene, game::GameObject* root);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  struct NodeRecord {
    std::unique_ptr<game::GameObject> owned;       // non-null while executed
    // cppcheck-suppress unusedStructMember
    game::GameObject*                 ptr;          // stable raw pointer
    // cppcheck-suppress unusedStructMember
    game::GameObject*                 orig_parent;  // null for root-level nodes
  };

  // Appends obj and all dynamic descendants in DFS pre-order to subtree_.
  void Collect(game::GameObject* obj);

  // Reclaims all nodes bottom-up (used by Execute and Redo).
  void ReclaimAll();

  // cppcheck-suppress unusedStructMember
  EditorScene* scene_;

  // DFS pre-order (root first). Execute / Redo process in reverse (bottom-up).
  // cppcheck-suppress unusedStructMember
  std::vector<NodeRecord> subtree_;
};

}  // namespace editor
