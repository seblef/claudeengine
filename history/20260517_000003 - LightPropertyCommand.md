# LightPropertyCommand — Undo/redo for light property edits

**Date:** 2026-05-17  
**Issue:** #237  
**Branch:** feat/light-property-command-237

---

## Goal

Make every light property edit in `PropertiesPanel` undoable (color, intensity, shadow settings, type-specific fields).

---

## Changes

### New files

| File | Role |
|---|---|
| `src/editor/commands/LightPropertyCommand.h` | `LightSnapshot` struct, `CaptureSnapshot` / `ApplySnapshot` helpers, `LightPropertyCommand` class |
| `src/editor/commands/LightPropertyCommand.cpp` | Implementations of all of the above |

### Modified files

| File | Change |
|---|---|
| `src/editor/PropertiesPanel.h` | Added `SetCommandHistory()`, `history_`, `before_snapshot_` members; changed `RenderLightProperties` to take non-const `game::GameLight*` |
| `src/editor/PropertiesPanel.cpp` | Inlined widgets (removed free helper functions), added `track()` lambda after every ImGui widget |
| `src/editor/EditorWindow.cpp` | `properties_panel_->SetCommandHistory(&history_)` added to constructor |
| `src/editor/commands/CMakeLists.txt` | Added `LightPropertyCommand.cpp` |

---

## Key decisions

### Snapshot approach (not per-field commands)

A single `LightSnapshot` captures all editable fields of all light subtypes at once. Unused fields are zero-initialised. This avoids a combinatorial explosion of command classes and keeps undo/redo granularity at the drag level (one entry per drag gesture), which is the right UX.

### `CaptureSnapshot` / `ApplySnapshot` as free functions

Both helpers are declared in `LightPropertyCommand.h` so `PropertiesPanel` can call them without depending on private command internals. The `switch` over `LightType` with `static_cast` is preferred over virtual dispatch here because the snapshot concept is editor-only and shouldn't pollute the renderer interface.

### Direction stored as `Vec3f`, not yaw/pitch

`LightSnapshot` stores `core::Vec3f direction` rather than the yaw/pitch degrees shown in the UI. This avoids a double conversion (degrees → radians → Vec3f on capture; Vec3f → yaw/pitch → radians on apply) that would accumulate floating-point error during rapid undo/redo.

### `track()` lambda in `RenderLightProperties`

Instead of refactoring the existing free helper functions to accept a context struct, the widgets are inlined into `RenderLightProperties` and a local `track()` lambda is called after each one. This keeps the tracking logic in one place and avoids threading extra parameters through every helper.

### `IsItemActivated` / `IsItemDeactivatedAfterEdit` pattern

- `IsItemActivated` fires on the first frame the user clicks a widget → snapshot taken before any change.
- `IsItemDeactivatedAfterEdit` fires when the user releases → snapshot taken after; command pushed only if before ≠ after.

This correctly handles DragFloat (continuous edits during drag), Checkbox (instant toggle), and Combo (instant selection).

### `SetCommandHistory` setter (not constructor argument)

`PropertiesPanel` keeps its zero-argument constructor so `EditorWindow` can construct it before calling the setter. This mirrors the pattern used by `EditorViewport` and `EditorToolbar`.

---

## What to keep in mind for future work

- `PropertiesPanel::RenderMeshProperties` has no undo support yet (mesh properties are read-only label for now, so no command needed).
- If more complex light types are added in future, `CaptureSnapshot` and `ApplySnapshot` need a matching `case` in their `switch`.
- `LightSnapshot::operator==` does exact float comparison. This is intentional: we only push a command if the value byte-for-byte differs from what was captured at drag-start.

---

## Skills / CLAUDE.md instructions followed

- `src/CLAUDE.md`: one class per `.h` / `.cpp` pair; Google C++ style; include root is `src/`.
- `src/editor/CLAUDE.md`: editor is the dependency leaf; one class per file pair.
- Root `CLAUDE.md`: conventional commits, history MD file, cpplint before commit.
