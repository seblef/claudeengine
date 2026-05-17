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
// The command AddRef's both before_ and after_ on construction and Release's
// them on destruction, keeping them alive for the lifetime of the command
// regardless of whether MeshTemplate drops its own reference on Execute/Undo.
class MaterialAssignCommand : public EditorCommand {
 public:
  MaterialAssignCommand(game::GameMesh*     mesh,
                        game::GameMaterial* before,
                        game::GameMaterial* after);
  ~MaterialAssignCommand() override;

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
