#include "editor/commands/DeleteObjectCommand.h"

#include "editor/EditorScene.h"
#include "game/GameObject.h"

namespace editor {

DeleteObjectCommand::DeleteObjectCommand(EditorScene* scene,
                                         game::GameObject* obj)
    : scene_(scene),
      owned_(nullptr),
      deleted_(obj),
      was_selected_(scene->IsSelected(obj)) {}

void DeleteObjectCommand::Execute() {
  // ReclaimDynamicObject already removes the object from the selection.
  owned_ = scene_->ReclaimDynamicObject(deleted_);
}

void DeleteObjectCommand::Undo() {
  scene_->AddDynamicObject(std::move(owned_));
  if (was_selected_)
    scene_->AddToSelection(deleted_);
}

void DeleteObjectCommand::Redo() {
  owned_ = scene_->ReclaimDynamicObject(deleted_);
}

std::string_view DeleteObjectCommand::GetDescription() const {
  return "Delete Object";
}

}  // namespace editor
