#include "editor/commands/TransformCommand.h"

#include "game/GameObject.h"

namespace editor {

TransformCommand::TransformCommand(game::GameObject* obj,
                                   const core::Mat4f& before,
                                   const core::Mat4f& after)
    : obj_(obj), before_(before), after_(after) {}

void TransformCommand::Execute() {
  obj_->SetWorldTransform(after_);
}

void TransformCommand::Undo() {
  obj_->SetWorldTransform(before_);
}

void TransformCommand::Redo() {
  obj_->SetWorldTransform(after_);
}

std::string_view TransformCommand::GetDescription() const {
  return "Transform Object";
}

}  // namespace editor
