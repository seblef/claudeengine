#include "editor/commands/PhysicsPropertyCommand.h"

#include "game/GameMesh.h"

namespace editor {

PhysicsPropertyCommand::PhysicsPropertyCommand(
    game::GameMesh*                         mesh,
    std::optional<physics::PhysicsBodyDesc> before,
    std::optional<physics::PhysicsBodyDesc> after)
    : mesh_(mesh),
      before_(std::move(before)),
      after_(std::move(after)) {}

void PhysicsPropertyCommand::Apply(
    const std::optional<physics::PhysicsBodyDesc>& desc) {
  if (desc)
    mesh_->SetPhysicsDesc(*desc);
  else
    mesh_->ClearPhysicsDesc();
}

void PhysicsPropertyCommand::Execute() { Apply(after_); }
void PhysicsPropertyCommand::Undo()    { Apply(before_); }
void PhysicsPropertyCommand::Redo()    { Apply(after_); }

std::string_view PhysicsPropertyCommand::GetDescription() const {
  return "Edit Physics Property";
}

}  // namespace editor
