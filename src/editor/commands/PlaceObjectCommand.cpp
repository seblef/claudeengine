#include "editor/commands/PlaceObjectCommand.h"

#include "editor/EditorScene.h"
#include "game/GameObject.h"

namespace editor {

PlaceObjectCommand::PlaceObjectCommand(EditorScene* scene,
                                       std::unique_ptr<game::GameObject> obj)
    : scene_(scene), owned_(std::move(obj)), placed_(nullptr) {}

void PlaceObjectCommand::Execute() {
  placed_ = scene_->AddDynamicObject(std::move(owned_));
  scene_->SetSelectedObject(placed_);
}

void PlaceObjectCommand::Undo() {
  owned_  = scene_->ReclaimDynamicObject(placed_);
  placed_ = nullptr;
}

void PlaceObjectCommand::Redo() {
  Execute();
}

std::string_view PlaceObjectCommand::GetDescription() const {
  return "Place Object";
}

}  // namespace editor
