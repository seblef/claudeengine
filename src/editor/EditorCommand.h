#pragma once

#include <string_view>

namespace editor {

// Abstract base for all reversible editor operations.
//
// Concrete commands implement Execute() and Undo(); Redo() defaults to
// re-invoking Execute(), but subclasses may override when a cheaper path
// exists.
class EditorCommand {
 public:
  virtual ~EditorCommand() = default;

  // Applies the operation.
  virtual void Execute() = 0;

  // Reverses the operation.
  virtual void Undo() = 0;

  // Re-applies the operation after an undo. Defaults to Execute().
  virtual void Redo() { Execute(); }

  // Returns a short human-readable label (e.g. "Move Object").
  virtual std::string_view GetDescription() const = 0;
};

}  // namespace editor
