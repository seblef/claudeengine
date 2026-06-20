#include "editor/commands/AddChildPivotCommand.h"

#include <memory>
#include <utility>

#include "editor/EditorScene.h"
#include "game/GamePivot.h"
#include "game/GameObject.h"

namespace editor {

AddChildPivotCommand::AddChildPivotCommand(EditorScene*      scene,
                                           std::string       name,
                                           game::GameObject* parent)
    : scene_(scene), name_(std::move(name)), parent_(parent) {}

void AddChildPivotCommand::Execute() {
  auto pivot = std::make_unique<game::GamePivot>();
  pivot->SetName(name_);
  placed_pivot_ = static_cast<game::GamePivot*>(
      scene_->AddDynamicObject(std::move(pivot)));
  if (parent_) placed_pivot_->Reparent(parent_);
  scene_->SetSelectedObject(placed_pivot_);
}

void AddChildPivotCommand::Undo() {
  auto raw = scene_->ReclaimDynamicObject(placed_pivot_);
  owned_pivot_.reset(static_cast<game::GamePivot*>(raw.release()));
  placed_pivot_ = nullptr;
  scene_->ClearSelection();
}

void AddChildPivotCommand::Redo() {
  placed_pivot_ = static_cast<game::GamePivot*>(
      scene_->AddDynamicObject(std::move(owned_pivot_)));
  if (parent_) placed_pivot_->Reparent(parent_);
  scene_->SetSelectedObject(placed_pivot_);
}

std::string_view AddChildPivotCommand::GetDescription() const {
  return "Add Pivot";
}

}  // namespace editor
