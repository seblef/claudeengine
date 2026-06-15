# Fix: Pasted Objects Land at World Origin Instead of Near Source

**Issue**: #521  
**Branch**: `fix/editor-paste-position-521`

## Problem

When using Copy/Paste in the editor, pasted objects appeared at a fixed position near the world origin `(1, 0, 1)` instead of near the source object.

## Root Cause

`Mat4f` uses row-major storage with `operator()(row, col)`.  Translation lives at **column 3** (rows 0–2):

```
| 1  0  0  tx |   → t(0,3) = tx
| 0  1  0  ty |   → t(1,3) = ty
| 0  0  1  tz |   → t(2,3) = tz
| 0  0  0  1  |   → t(3,x) = 0 (last row, always 0/1)
```

Both `CopySelectedObject()` and `PasteObject()` were reading **row 3** (`t(3,0)`, `t(3,1)`, `t(3,2)`), which is always `(0, 0, 0)` for a standard transform. The clipboard was therefore created at the world origin, and every paste landed at `(1, 0, 1)`.

## Fix

`src/editor/EditorWindow.cpp`:

- `CopySelectedObject()`: `t(3,0), t(3,1), t(3,2)` → `t(0,3), t(1,3), t(2,3)`
- `PasteObject()`: `ct(3,0), ct(3,1), ct(3,2)` → `ct(0,3), ct(1,3), ct(2,3)`

The small `+1` offset on X and Z in `PasteObject()` is kept so consecutive pastes don't stack on top of each other.

## Notes for Future Work

- `FallToTerrain()` in the same file already used the correct `(0,3)` / `(2,3)` indexing — the inconsistency was localized to the copy/paste path.
- No existing tests cover editor-level copy/paste; a future test could instantiate `EditorWindow` in isolation and verify `PasteObject()` produces a clone near the source.

## Skills / CLAUDE.md rules applied

- Checked out `dev`, pulled, created `fix/` branch before implementing.
- cpplint passed with no warnings.
- History file written to `history/`.
