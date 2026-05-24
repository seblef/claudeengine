#include "editor/EditorCommandHistory.h"

#include <utility>

namespace editor {

EditorCommandHistory::EditorCommandHistory(int max_size)
    : max_size_(max_size) {}

void EditorCommandHistory::Push(std::unique_ptr<EditorCommand> cmd) {
  cmd->Execute();
  undo_stack_.push_back(std::move(cmd));
  if (static_cast<int>(undo_stack_.size()) > max_size_)
    undo_stack_.pop_front();
  redo_stack_.clear();
  if (on_dirty_) on_dirty_();
  if (on_scene_modified_) on_scene_modified_();
}

void EditorCommandHistory::Undo() {
  if (undo_stack_.empty()) return;
  auto cmd = std::move(undo_stack_.back());
  undo_stack_.pop_back();
  cmd->Undo();
  redo_stack_.push_back(std::move(cmd));
  if (on_scene_modified_) on_scene_modified_();
}

void EditorCommandHistory::Redo() {
  if (redo_stack_.empty()) return;
  auto cmd = std::move(redo_stack_.back());
  redo_stack_.pop_back();
  cmd->Redo();
  undo_stack_.push_back(std::move(cmd));
  if (on_scene_modified_) on_scene_modified_();
}

bool EditorCommandHistory::CanUndo() const {
  return !undo_stack_.empty();
}

bool EditorCommandHistory::CanRedo() const {
  return !redo_stack_.empty();
}

void EditorCommandHistory::Clear() {
  undo_stack_.clear();
  redo_stack_.clear();
}

}  // namespace editor
