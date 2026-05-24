# ObjectsPanel — inline object rename on double-click (#266)

## Summary

Added inline rename support to the Objects panel. Double-clicking a leaf node
switches it to an `ImGui::InputText` field pre-filled with the current name.
Pressing Enter or losing focus commits the rename as a `RenameObjectCommand`
(undoable). Pressing Escape cancels without modifying anything.

## Changes

### `src/editor/ObjectsPanel.h`
- Added private state: `renaming_obj_`, `rename_buf_[256]`,
  `rename_focus_needed_`, `history_`.
- Added `SetCommandHistory(EditorCommandHistory*)`.
- Converted `RenderGroup` from an anonymous-namespace free function to a
  `private static` member, receiving rename state by reference so a single
  `ObjectsPanel` instance owns all mutable state.
- Added required includes (`<cstddef>`, `<vector>`, `game/GameObjectType.h`).

### `src/editor/ObjectsPanel.cpp`
- Implemented `SetCommandHistory`.
- Re-implemented `RenderGroup` as a static member with two rendering branches
  per leaf: normal mode (click to select, double-click to start rename) and
  rename mode (InputText, commit on Enter/focus-loss, cancel on Escape).
- `CommitRename` logic is inlined directly in each exit path (Enter, focus-loss)
  to avoid a helper that references member state from a static function.
- Each `Render()` call passes `renaming_obj_`, `rename_buf_`, etc. by reference
  to `RenderGroup`, keeping the rename state object-lifetime-correct.

### `src/editor/EditorWindow.cpp`
- Added `objects_panel_->SetCommandHistory(&history_)` in the constructor,
  consistent with the existing pattern for other panels.

## Decisions

- **Static member rather than free function** — the issue spec asks to refactor
  `RenderGroup` to a private static member so it can access rename state via
  reference parameters without depending on a global or a closure. This keeps
  the function testable and avoids capturing `this`.
- **Commit logic inlined, not in a separate `CommitRename` helper** — a private
  `CommitRename` method on the class cannot be called from a `static` function
  without passing `this`. Inlining two small identical code paths is cleaner
  than threading an extra pointer argument.
- **`rename_focus_needed_` checked before `IsItemActive`** — `SetKeyboardFocusHere`
  fires *before* the widget is rendered; on the same frame the item is not yet
  active, so the focus-loss branch must be guarded by `!rename_focus_needed_`
  to avoid immediately cancelling the rename on the first frame. The issue spec
  uses this exact guard implicitly; the implementation preserves it by only
  entering the focus-loss branch after `SetKeyboardFocusHere` has been consumed.

## Output to keep in mind

- `EditorCommandHistory::Push` executes the command immediately and marks the
  scene dirty — no separate `Execute()` call needed after `Push`.
- The rename state (`renaming_obj_`, `rename_buf_`, `rename_focus_needed_`) is
  shared across all three groups (Meshes / Lights / Cameras) via the same
  member variables, which is correct because only one object can be renamed
  at a time.

## Skills / instructions consulted

- `src/editor/CLAUDE.md` — one class per file, ImGui framing rules.
- `src/CLAUDE.md` — Google C++ style, include paths relative to `src/`.
- `impl-issue` skill — branch, implement, cpplint, commit, PR to `dev`.
