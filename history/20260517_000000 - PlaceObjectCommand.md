# PlaceObjectCommand — undoable object placement

**Date:** 2026-05-17  
**Issue:** #234  
**Branch:** feat/place-object-command-234  

---

## Summary

Makes clicking to place a mesh or light in the viewport fully undoable via
`Ctrl+Z` / `Ctrl+Shift+Z`. Every confirmed placement is now recorded as a
`PlaceObjectCommand` in the shared `EditorCommandHistory`.

---

## Changes

### New files

| File | Responsibility |
|---|---|
| `src/editor/commands/PlaceObjectCommand.h` | Declares `PlaceObjectCommand` |
| `src/editor/commands/PlaceObjectCommand.cpp` | Implements Execute / Undo / Redo |
| `src/editor/commands/CMakeLists.txt` | Appends `PlaceObjectCommand.cpp` to the `editor` target |

### Modified files

| File | Change |
|---|---|
| `src/editor/EditorScene.h` | Added `ReclaimDynamicObject()` declaration |
| `src/editor/EditorScene.cpp` | Implemented `ReclaimDynamicObject()` |
| `src/editor/EditorViewport.h` | Added `SetCommandHistory()` and `history_` member |
| `src/editor/EditorViewport.cpp` | Added `PlaceObjectCommand` include; rewired `PlacePreview()` to push a command |
| `src/editor/EditorWindow.cpp` | Wires `viewport_->SetCommandHistory(&history_)` in constructor |

---

## Design decisions

### ReclaimDynamicObject

`PlacePreview()` is called after the preview object has already been added to
the scene (via `UpdatePreviewPosition`). To wrap the placement in a command,
the object must be extracted from the scene first so the command can hold
ownership. `ReclaimDynamicObject` does this: it mirrors `RemoveDynamicObject`
but moves the `unique_ptr` out of `dynamic_objects_` and returns it instead of
deleting it.

### Ownership invariant in PlaceObjectCommand

At any point in time, exactly one of `owned_` / `placed_` is non-null:

| State | `owned_` | `placed_` |
|---|---|---|
| Just constructed (before Execute) | set | nullptr |
| After Execute / Redo | nullptr | set |
| After Undo | set | nullptr |

### CMake subdirectory pattern

`commands/CMakeLists.txt` uses `target_sources(editor PRIVATE ...)` rather
than defining a separate library, avoiding a circular dependency
(`editor_commands` would need `EditorScene` which lives in `editor`).

### Fallback without history

`PlacePreview()` falls back to the old direct-placement path if `history_` is
null, keeping the viewport usable in isolation (e.g., unit tests that don't
construct a full `EditorWindow`).

---

## Output to keep in mind

- `ReclaimDynamicObject` is the canonical way to transfer ownership of a
  dynamic scene object back to a caller; use it for any future undo command
  that needs to remove an already-placed object.
- The `commands/` subdirectory pattern (`target_sources` on the parent target)
  is the established approach for adding new command classes — no new CMake
  library needed.
- `PlaceMeshAt` (drag-and-drop flow) is **not** wrapped in a command yet;
  that is a separate issue.
