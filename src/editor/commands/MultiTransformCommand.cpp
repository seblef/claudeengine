#include "editor/commands/MultiTransformCommand.h"

#include <utility>

#include "game/GameObject.h"

namespace editor {

MultiTransformCommand::MultiTransformCommand(std::vector<Entry> entries)
    : entries_(std::move(entries)) {}

void MultiTransformCommand::Execute() {
  for (const auto& e : entries_)
    e.obj->SetWorldTransform(e.after);
}

void MultiTransformCommand::Undo() {
  for (const auto& e : entries_)
    e.obj->SetWorldTransform(e.before);
}

void MultiTransformCommand::Redo() {
  for (const auto& e : entries_)
    e.obj->SetWorldTransform(e.after);
}

std::string_view MultiTransformCommand::GetDescription() const {
  return "Transform Objects";
}

}  // namespace editor
