#include "editor/commands/RenameObjectCommand.h"

#include <utility>

#include "game/GameObject.h"

namespace editor {

RenameObjectCommand::RenameObjectCommand(game::GameObject* obj,
                                         std::string old_name,
                                         std::string new_name)
    : obj_(obj),
      old_name_(std::move(old_name)),
      new_name_(std::move(new_name)) {}

void RenameObjectCommand::Execute() {
  obj_->SetName(new_name_);
}

void RenameObjectCommand::Undo() {
  obj_->SetName(old_name_);
}

std::string_view RenameObjectCommand::GetDescription() const {
  return "Rename Object";
}

}  // namespace editor
