#pragma once

#include <memory>
#include <string_view>

#include "editor/EditorCommand.h"

namespace game {
class GameObject;
}  // namespace game

namespace editor {

class EditorScene;

// Command that places a game object into the scene, with full undo/redo support.
//
// Ownership invariant: exactly one of owned_ or placed_ is non-null at any
// time. owned_ holds the object while undone; placed_ is the raw pointer while
// executed.
class PlaceObjectCommand : public EditorCommand {
 public:
  PlaceObjectCommand(EditorScene* scene,
                     std::unique_ptr<game::GameObject> obj);

  void Execute() override;
  void Undo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  EditorScene*                      scene_;
  std::unique_ptr<game::GameObject> owned_;   // non-null when undone
  // cppcheck-suppress unusedStructMember
  game::GameObject*                 placed_;  // non-null when executed
};

}  // namespace editor
