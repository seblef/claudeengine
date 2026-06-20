#include "editor/commands/UngroupPivotCommand.h"

#include <string_view>

#include "editor/EditorScene.h"
#include "game/GamePivot.h"
#include "game/GameObject.h"

namespace editor {

UngroupPivotCommand::UngroupPivotCommand(EditorScene* scene, game::GamePivot* pivot)
    : scene_(scene),
      placed_pivot_(pivot),
      children_(pivot->GetChildren()),
      pivot_parent_(pivot->GetParent()) {}

void UngroupPivotCommand::Execute() {
  owned_pivot_ = scene_->UngroupPivot(placed_pivot_);
  placed_pivot_ = nullptr;
  if (!children_.empty())
    scene_->SetSelectedObject(children_[0]);
  else
    scene_->ClearSelection();
}

void UngroupPivotCommand::Undo() {
  // Re-add the pivot to the scene and reparent children back under it.
  game::GamePivot* raw = static_cast<game::GamePivot*>(
      scene_->AddDynamicObject(std::move(owned_pivot_)));
  raw->Reparent(pivot_parent_);
  for (auto* child : children_)
    child->Reparent(raw);
  placed_pivot_ = raw;
  scene_->SetSelectedObject(raw);
}

std::string_view UngroupPivotCommand::GetDescription() const {
  return "Ungroup pivot";
}

}  // namespace editor
