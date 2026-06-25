#include "editor/commands/RoadMaterialAssignCommand.h"

#include "game/GameMaterial.h"
#include "game/GameRoad.h"

namespace editor {

RoadMaterialAssignCommand::RoadMaterialAssignCommand(game::GameRoad*     road,
                                                     game::GameMaterial* before,
                                                     game::GameMaterial* after)
    : road_(road), before_(before), after_(after) {
  if (before_) before_->AddRef();
  if (after_)  after_->AddRef();
}

RoadMaterialAssignCommand::~RoadMaterialAssignCommand() {
  if (before_) before_->Release();
  if (after_)  after_->Release();
}

void RoadMaterialAssignCommand::Execute() {
  road_->SetMaterial(after_);
}

void RoadMaterialAssignCommand::Undo() {
  road_->SetMaterial(before_);
}

void RoadMaterialAssignCommand::Redo() {
  road_->SetMaterial(after_);
}

std::string_view RoadMaterialAssignCommand::GetDescription() const {
  return "Assign Road Material";
}

}  // namespace editor
