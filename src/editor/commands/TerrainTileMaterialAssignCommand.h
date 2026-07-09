#pragma once

#include <string_view>

#include "editor/EditorCommand.h"

namespace game {
class GameTerrainTile;
class GameMaterial;
}  // namespace game

namespace editor {

// Undoable command that reassigns a material on a GameTerrainTile.
//
// Null-safe: before_ or after_ may be nullptr (tile had no material assigned).
// AddRef is called on non-null pointers in the constructor; Release is called
// in the destructor, keeping the materials alive for the duration of undo history.
class TerrainTileMaterialAssignCommand : public EditorCommand {
 public:
  TerrainTileMaterialAssignCommand(game::GameTerrainTile* tile,
                                   game::GameMaterial*     before,
                                   game::GameMaterial*     after);
  ~TerrainTileMaterialAssignCommand() override;

  void Execute() override;
  void Undo()    override;
  void Redo()    override;
  std::string_view GetDescription() const override;

 private:
  // cppcheck-suppress unusedStructMember
  game::GameTerrainTile* tile_;
  // cppcheck-suppress unusedStructMember
  game::GameMaterial*    before_;
  // cppcheck-suppress unusedStructMember
  game::GameMaterial*    after_;
};

}  // namespace editor
