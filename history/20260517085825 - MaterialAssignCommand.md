# MaterialAssignCommand — Undoable Material Assignment (#240)

## Summary

Added `MaterialAssignCommand`, making the "Apply to selection" button in
`MaterialEditorWindow` fully undoable via the existing `EditorCommandHistory`
infrastructure.

## Changes

### New files

| File | Role |
|---|---|
| `src/editor/commands/MaterialAssignCommand.h` | Command declaration |
| `src/editor/commands/MaterialAssignCommand.cpp` | Command implementation |

### Modified files

| File | Change |
|---|---|
| `src/editor/commands/CMakeLists.txt` | Added `MaterialAssignCommand.cpp` to sources |
| `src/editor/MaterialEditorWindow.cpp` | Include new header; updated `ApplyToSelection` to push command |

## Decisions and rationale

### Material pointer, not snapshot

`MaterialPropertyCommand` stores full property snapshots because the command
edits the *content* of a material.  `MaterialAssignCommand` only swaps *which*
material pointer is stored on the `MeshTemplate`; a plain `GameMaterial*` is
sufficient.  Both pointers are owned by `EditorScene::game_materials_` for the
session lifetime, so no ownership transfer or ref-counting is needed.

### Delegation through MeshTemplate

`GameMesh` has no `SetMaterial()` method; the assignment is done via
`mesh->GetTemplate()->SetMaterial()`.  The issue's pseudocode used
`mesh->SetMaterial()` as a shorthand; the implementation targets the actual API.

### Graceful no-history path

When `history_` is `nullptr` (i.e., `SetCommandHistory` was never called),
`ApplyToSelection` falls back to a direct `SetMaterial()` call so the window
remains functional in contexts without undo/redo.  This mirrors the pattern
used by `MaterialPropertyCommand` in `RenderRenderingSection`.

## Output to keep in mind

- `EditorCommandHistory::Push` calls `Execute()` automatically; commands must
  not apply their effect in the constructor.
- The `else` fallback is the only path that calls `SetMaterial` directly when
  there is no history registered.

## Skills and instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md` — one class per `.h`/`.cpp` pair, ImGui lifecycle rules
- `src/CLAUDE.md` — Google C++ style, include root at `src/`
- `CLAUDE.md` (root) — cpplint, conventional commits, PR to `dev`
