#include "editor/commands/GroupUnderPivotCommand.h"

#include <string_view>
#include <utility>

#include "editor/EditorScene.h"
#include "game/GamePivot.h"
#include "game/GameObject.h"

namespace editor {

GroupUnderPivotCommand::GroupUnderPivotCommand(
    EditorScene*                   scene,
    std::string                    pivot_name,
    std::vector<game::GameObject*> children)
    : scene_(scene),
      pivot_name_(std::move(pivot_name)),
      children_(std::move(children)) {}

void GroupUnderPivotCommand::Execute() {
  placed_pivot_ = scene_->GroupUnderNewPivot(pivot_name_, children_);
  scene_->SetSelectedObject(placed_pivot_);
}

void GroupUnderPivotCommand::Undo() {
  owned_pivot_ = scene_->UngroupPivot(placed_pivot_);
  placed_pivot_ = nullptr;
  if (!children_.empty())
    scene_->SetSelectedObject(children_[0]);
  else
    scene_->ClearSelection();
}

void GroupUnderPivotCommand::Redo() {
  // Place the already-created pivot back and reparent children.
  placed_pivot_ = static_cast<game::GamePivot*>(
      scene_->AddDynamicObject(std::move(owned_pivot_)));
  for (auto* child : children_)
    child->Reparent(placed_pivot_);
  scene_->SetSelectedObject(placed_pivot_);
}

std::string_view GroupUnderPivotCommand::GetDescription() const {
  return "Group under pivot";
}

}  // namespace editor
