#include "editor/commands/MaterialAssignCommand.h"

#include "game/GameMesh.h"
#include "game/GameMaterial.h"
#include "game/MeshTemplate.h"

namespace editor {

MaterialAssignCommand::MaterialAssignCommand(game::GameMesh*     mesh,
                                             game::GameMaterial* before,
                                             game::GameMaterial* after)
    : mesh_(mesh), before_(before), after_(after) {}

void MaterialAssignCommand::Execute() {
  mesh_->GetTemplate()->SetMaterial(after_);
}

void MaterialAssignCommand::Undo() {
  mesh_->GetTemplate()->SetMaterial(before_);
}

void MaterialAssignCommand::Redo() {
  mesh_->GetTemplate()->SetMaterial(after_);
}

std::string_view MaterialAssignCommand::GetDescription() const {
  return "Assign Material";
}

}  // namespace editor
