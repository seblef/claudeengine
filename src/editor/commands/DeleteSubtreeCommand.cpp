#include "editor/commands/DeleteSubtreeCommand.h"

#include <utility>

#include "editor/EditorScene.h"
#include "game/GameObject.h"

namespace editor {

DeleteSubtreeCommand::DeleteSubtreeCommand(EditorScene*      scene,
                                           game::GameObject* root)
    : scene_(scene) {
  Collect(root);
}

void DeleteSubtreeCommand::Collect(game::GameObject* obj) {
  if (!scene_->IsDynamic(obj)) return;
  subtree_.push_back({nullptr, obj, obj->GetParent()});
  // Snapshot children — Reparent calls during ReclaimAll must not invalidate.
  const std::vector<game::GameObject*> children = obj->GetChildren();
  for (auto* child : children)
    Collect(child);
}

void DeleteSubtreeCommand::ReclaimAll() {
  // Process bottom-up so each node's children are already gone when reclaimed.
  // ReclaimDynamicObject calls DetachChildren (no-op) then Reparent(nullptr).
  for (int i = static_cast<int>(subtree_.size()) - 1; i >= 0; --i)
    subtree_[i].owned = scene_->ReclaimDynamicObject(subtree_[i].ptr);
  scene_->ClearSelection();
}

void DeleteSubtreeCommand::Execute() {
  ReclaimAll();
}

void DeleteSubtreeCommand::Undo() {
  // Re-add top-down so parents are in the scene before children are reparented.
  for (auto& rec : subtree_) {
    scene_->AddDynamicObject(std::move(rec.owned));
    if (rec.orig_parent)
      rec.ptr->Reparent(rec.orig_parent);
  }
}

void DeleteSubtreeCommand::Redo() {
  ReclaimAll();
}

std::string_view DeleteSubtreeCommand::GetDescription() const {
  return "Delete Subtree";
}

}  // namespace editor
