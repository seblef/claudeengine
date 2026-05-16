#pragma once

#include <deque>
#include <memory>

#include "editor/EditorCommand.h"

namespace editor {

// Manages undo/redo stacks for editor operations.
//
// Push() executes a command and adds it to the undo stack. Undo()/Redo()
// shuttle commands between the two stacks. The history is bounded by
// max_size; when the undo stack exceeds it the oldest entry is dropped.
class EditorCommandHistory {
 public:
  static constexpr int kDefaultMaxSize = 100;

  explicit EditorCommandHistory(int max_size = kDefaultMaxSize);

  // Executes cmd, appends it to the undo stack, and clears the redo stack.
  void Push(std::unique_ptr<EditorCommand> cmd);

  // Undoes the most recent command and moves it to the redo stack.
  void Undo();

  // Re-applies the most recently undone command and moves it to the undo stack.
  void Redo();

  bool CanUndo() const;
  bool CanRedo() const;

  // Empties both stacks (e.g. on scene reset).
  void Clear();

 private:
  // cppcheck-suppress unusedStructMember
  int max_size_;
  // cppcheck-suppress unusedStructMember
  std::deque<std::unique_ptr<EditorCommand>> undo_stack_;
  // cppcheck-suppress unusedStructMember
  std::deque<std::unique_ptr<EditorCommand>> redo_stack_;
};

}  // namespace editor
