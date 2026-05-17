#include "editor/commands/DeleteObjectCommand.h"

#include "editor/EditorScene.h"
#include "game/GameObject.h"

namespace editor {

DeleteObjectCommand::DeleteObjectCommand(EditorScene* scene,
                                         game::GameObject* obj)
    : scene_(scene),
      owned_(nullptr),
      deleted_(obj),
      was_selected_(scene->GetSelectedObject() == obj) {}

void DeleteObjectCommand::Execute() {
  owned_ = scene_->ReclaimDynamicObject(deleted_);
  if (was_selected_)
    scene_->SetSelectedObject(nullptr);
}

void DeleteObjectCommand::Undo() {
  scene_->AddDynamicObject(std::move(owned_));
  if (was_selected_)
    scene_->SetSelectedObject(deleted_);
}

void DeleteObjectCommand::Redo() {
  owned_ = scene_->ReclaimDynamicObject(deleted_);
  if (was_selected_)
    scene_->SetSelectedObject(nullptr);
}

std::string_view DeleteObjectCommand::GetDescription() const {
  return "Delete Object";
}

}  // namespace editor
