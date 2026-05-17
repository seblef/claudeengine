#pragma once

#include <memory>
#include <string_view>

#include "editor/EditorCommand.h"

namespace game {
class GameObject;
}  // namespace game

namespace editor {

class EditorScene;

// Command that removes a dynamic object from the scene, with full undo/redo support.
//
// Ownership invariant: owned_ is populated while "executed" (object is deleted),
// and null while "undone" (object is restored to the scene).
class DeleteObjectCommand : public EditorCommand {
 public:
  DeleteObjectCommand(EditorScene* scene, game::GameObject* obj);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  EditorScene*                      scene_;
  // Holds ownership of the object while it is deleted (Execute/Redo state).
  // cppcheck-suppress unusedStructMember
  std::unique_ptr<game::GameObject> owned_;
  // Raw pointer used to re-delete after Undo; stable because owned_ keeps it alive.
  // cppcheck-suppress unusedStructMember
  game::GameObject*                 deleted_;
  // cppcheck-suppress unusedStructMember
  bool                              was_selected_;
};

}  // namespace editor
