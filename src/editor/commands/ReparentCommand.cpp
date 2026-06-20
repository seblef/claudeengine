#include "editor/commands/ReparentCommand.h"

#include "game/GameObject.h"

namespace editor {

ReparentCommand::ReparentCommand(game::GameObject* obj,
                                 game::GameObject* new_parent)
    : obj_(obj),
      old_parent_(obj->GetParent()),
      new_parent_(new_parent) {}

void ReparentCommand::Execute() {
  obj_->Reparent(new_parent_);
}

void ReparentCommand::Undo() {
  obj_->Reparent(old_parent_);
}

std::string_view ReparentCommand::GetDescription() const {
  return "Reparent Object";
}

}  // namespace editor
