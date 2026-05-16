# Editor Command Pattern — EditorCommand + EditorCommandHistory

**Date:** 2026-05-16
**Issue:** #232
**Branch:** feat/editor-command-pattern-232

## Summary

Introduces the foundational undo/redo infrastructure for the editor. This is a
purely additive change — no existing code is modified except `EditorWindow.h`
and `CMakeLists.txt`.

## New Files

### `src/editor/EditorCommand.h`

Abstract base class for all reversible editor operations. Interface:

```cpp
class EditorCommand {
  virtual void Execute() = 0;
  virtual void Undo()    = 0;
  virtual void Redo()    { Execute(); }  // default: re-run Execute
  virtual std::string_view GetDescription() const = 0;
};
```

`Redo()` defaults to `Execute()` so that simple stateless commands need not
override it. Stateful commands (e.g. those capturing a before/after snapshot)
may override `Redo()` when a faster path exists.

### `src/editor/EditorCommandHistory.h/.cpp`

Stack manager wrapping two `std::deque<std::unique_ptr<EditorCommand>>` queues:

| Member | Behaviour |
|---|---|
| `Push(cmd)` | Calls `Execute()`, appends to undo stack, clears redo stack, trims oldest if `max_size` exceeded |
| `Undo()` | Pops undo stack → `Undo()` → appends to redo stack |
| `Redo()` | Pops redo stack → `Redo()` → appends to undo stack |
| `CanUndo()` / `CanRedo()` | Simple size checks |
| `Clear()` | Empties both stacks (called on scene reset) |

Default `max_size` is 100 (compile-time constant `kDefaultMaxSize`).

## Modified Files

### `src/editor/EditorWindow.h`

Added `EditorCommandHistory history_` as a direct member (not a pointer — no
heap allocation needed). Subsequent panels will receive `&history_` as a raw
pointer when they need it (follow-up issues).

### `src/editor/CMakeLists.txt`

Added `EditorCommandHistory.cpp` to the `editor` static library.

## Decisions & Rationale

- **`std::deque` over `std::stack`**: `deque` gives O(1) `pop_front` for
  trimming the oldest entry without an extra reverse.
- **Value member in `EditorWindow`** rather than `unique_ptr`: the history has
  the same lifetime as the window; heap indirection adds no benefit.
- **No keyboard shortcuts or toolbar buttons**: deferred to the next issue as
  specified.

## Output to Keep in Mind

- Panels that need undo/redo receive `EditorCommandHistory*` (raw pointer) from
  `EditorWindow`. Do not make `EditorCommandHistory` a singleton.
- `EditorCommand::Redo()` has a working default; only override when a cheaper
  path exists.
- The undo stack is trimmed from the **front** (oldest entry) when
  `max_size` is exceeded.

## Skills / Instructions Used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h`/`.cpp`, Google style, include root is `src/`
- `src/editor/CLAUDE.md`: one class per pair, editor is the dependency-graph leaf
