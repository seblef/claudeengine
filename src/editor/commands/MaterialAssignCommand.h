#pragma once

#include <string_view>

#include "editor/EditorCommand.h"

namespace game {
class GameMesh;
class GameMaterial;
}  // namespace game

namespace editor {

// Undoable command that reassigns a material on a GameMesh's template.
//
// Both before_ and after_ are expected to outlive this command (they are owned
// by EditorScene::game_materials_ for the editor session lifetime).
class MaterialAssignCommand : public EditorCommand {
 public:
  MaterialAssignCommand(game::GameMesh*     mesh,
                        game::GameMaterial* before,
                        game::GameMaterial* after);

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  game::GameMesh*     mesh_;
  // cppcheck-suppress unusedStructMember
  game::GameMaterial* before_;
  // cppcheck-suppress unusedStructMember
  game::GameMaterial* after_;
};

}  // namespace editor
