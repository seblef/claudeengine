# DeleteObjectCommand — Undo/redo for object deletion (issue #235)

## Changes

| File | Action |
|---|---|
| `src/editor/commands/DeleteObjectCommand.h` | New: header for `DeleteObjectCommand` |
| `src/editor/commands/DeleteObjectCommand.cpp` | New: implementation |
| `src/editor/commands/CMakeLists.txt` | Added `DeleteObjectCommand.cpp` as source |
| `src/editor/EditorViewport.cpp` | Wired Delete-key handler through `history_->Push(DeleteObjectCommand)` |

## Decisions and rationale

### Ownership invariant mirrors PlaceObjectCommand

`owned_` holds the `unique_ptr` while the object is deleted (Execute/Redo state), and is null while it lives in the scene (Undo state). This is symmetric with `PlaceObjectCommand`, making the pair easy to reason about together.

### Selection captured in constructor, not Execute()

`was_selected_` is computed once in the constructor (`scene->GetSelectedObject() == obj`), before any state changes. This is safe because `DeleteObjectCommand` is constructed immediately before being pushed to the history and executed — the selection cannot change between construction and `Execute()`.

### Explicit Redo() override

The base class `Redo()` delegates to `Execute()`, but the issue specification calls for an explicit `Redo()` override to keep the intent clear and because `Execute()` is called once (on first Push) while `Redo()` may be called repeatedly. Both do the same work here, but keeping them separate avoids surprising re-entrant side effects if `Execute()` ever gains construction-time logic.

### `selected_object_` reset in EditorViewport::OnEvent

The viewport's local `selected_object_` cache is cleared to `nullptr` when the delete command is pushed. The command itself clears `scene_->GetSelectedObject()` (via `ReclaimDynamicObject`), so both the scene and the viewport view stay consistent.

## Output to keep in mind

- `RemoveDynamicObject` is still present on `EditorScene` for the cancel-preview path (`CancelPreview()`), which correctly bypasses the command history — that is not a user-driven deletion.
- The undo/redo toolbar buttons and Ctrl+Z / Ctrl+Shift+Z shortcuts (from issue #233) automatically pick up `DeleteObjectCommand` because they drive `EditorCommandHistory`, which is shared.

## Skills / instructions referenced

- `impl-issue` skill workflow
- `src/CLAUDE.md`: one class per `.h` / `.cpp`, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md`: one class per file
- Root `CLAUDE.md`: git workflow, conventional commits, history file requirement
