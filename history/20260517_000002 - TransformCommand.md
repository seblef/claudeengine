# TransformCommand — Undoable Gizmo Transforms (issue #236)

## Summary

Added `TransformCommand` to record ImGuizmo-driven translate/rotate/scale operations
as reversible editor commands, making them undoable via `EditorCommandHistory`.

## New files

| File | Purpose |
|---|---|
| `src/editor/commands/TransformCommand.h` | Command declaration |
| `src/editor/commands/TransformCommand.cpp` | Command implementation |

## Changes

### `src/editor/commands/TransformCommand`

Stores `before_` and `after_` world transforms plus a raw pointer to the
`GameObject`. `Execute()` / `Redo()` apply `after_`; `Undo()` restores `before_`.
The command owns no resources — the object lifetime is managed by `EditorScene`.

### `src/editor/EditorViewport`

Two new private members track gizmo drag state across frames:
- `bool gizmo_was_using_` — previous-frame `ImGuizmo::IsUsing()` result
- `core::Mat4f gizmo_before_transform_` — world transform captured at drag-start

Logic added after `ImGuizmo::Manipulate()`:
1. **Drag start** (`!gizmo_was_using_ && gizmo_using`): snapshot `before_` transform.
2. **During drag** (`gizmo_using`): apply the manipulated matrix as before.
3. **Drag end** (`gizmo_was_using_ && !gizmo_using`): compare `before_` vs current
   transform; push a `TransformCommand` only when the transform actually changed.

`gizmo_was_using_` is reset to `false` whenever there is no selected object or no
active gizmo tool, preventing stale state when selection changes mid-drag.

## Decisions

- **Only the net delta is recorded.** ImGuizmo mutates the object every frame during
  the drag; storing only start/end avoids filling the history with hundreds of tiny
  incremental commands.
- **No-op guard.** The command is only pushed when `after != before`, preventing
  spurious history entries from accidental clicks on the gizmo.
- **`history_` null check.** The push is guarded by `history_`, consistent with the
  existing placement/deletion commands.

## Output to keep in mind

- `EditorViewport` now depends on `TransformCommand` — add the include path if
  `EditorViewport` is ever split into a thinner wrapper.
- The `gizmo_before_transform_` member is default-constructed (identity) and only
  meaningful after a drag-start event; this is safe because it is always written
  before it is read.

## Skills / instructions followed

- `src/CLAUDE.md`: one class per `.h` / `.cpp` pair; Google C++ style; include root
  is `src/`.
- `src/editor/CLAUDE.md`: editor is the leaf module; no singletons.
- Issue #236 specification: `TransformCommand` API and wiring pseudocode followed
  exactly.
