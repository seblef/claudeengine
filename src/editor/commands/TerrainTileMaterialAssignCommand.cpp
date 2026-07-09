#include "editor/commands/TerrainTileMaterialAssignCommand.h"

#include "game/GameMaterial.h"
#include "game/GameTerrainTile.h"

namespace editor {

TerrainTileMaterialAssignCommand::TerrainTileMaterialAssignCommand(
    game::GameTerrainTile* tile,
    game::GameMaterial*    before,
    game::GameMaterial*    after)
    : tile_(tile), before_(before), after_(after) {
  if (before_) before_->AddRef();
  if (after_)  after_->AddRef();
}

TerrainTileMaterialAssignCommand::~TerrainTileMaterialAssignCommand() {
  if (before_) before_->Release();
  if (after_)  after_->Release();
}

void TerrainTileMaterialAssignCommand::Execute() {
  tile_->SetMaterial(after_);
}

void TerrainTileMaterialAssignCommand::Undo() {
  tile_->SetMaterial(before_);
}

void TerrainTileMaterialAssignCommand::Redo() {
  tile_->SetMaterial(after_);
}

std::string_view TerrainTileMaterialAssignCommand::GetDescription() const {
  return "Assign Terrain Tile Material";
}

}  // namespace editor
