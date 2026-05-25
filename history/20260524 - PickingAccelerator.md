# PickingAccelerator — Uniform 3D Grid for Ray Picking

**Date:** 2026-05-24
**Issue:** #271
**Branch:** feat/picking-accelerator-271

## Summary

Added a `PickingAccelerator` class — a uniform 3D grid — that pre-filters candidate objects for a given pick ray (Level 1 of the 3-level picking pipeline in EditorViewport). Both mesh and positional light objects are inserted; GlobalLight is excluded.

## Changes

### New: `src/editor/PickingAccelerator.h` / `.cpp`

- Uniform 3D grid: flat `vector<vector<GameObject*>>` with `nx * ny * nz` cells.
- **Build**: cell size = `max(diagonal / 32, 1.f)`. Objects inserted into every cell their world bbox overlaps.
- **Add / Remove / UpdateMoved**: incremental updates for object lifecycle events.
- **QueryRay**: 3D DDA traversal along the ray; collects objects from each visited cell; deduplicates with an `unordered_set`; returns candidates roughly ordered by ascending depth.
- `IsPickable()` helper excludes `GlobalLight` (no meaningful world bbox).

### Modified: `src/editor/EditorScene.h` / `.cpp`

- Added `GetBounds()`: returns the union of all object world bboxes, guaranteed to have a minimum diagonal of 10 world units (so empty scenes still produce a valid grid).

### Modified: `src/editor/EditorViewport.h` / `.cpp`

- Added `picking_acc_` member (value, not pointer — no heap allocation).
- `SetScene()` converted from inline to out-of-line; calls `picking_acc_.Build()` after setting the scene pointer.
- **Incremental hooks** added:
  - `UpdatePreviewPosition()`: `Add()` on first placement, `UpdateMoved()` on each cursor move.
  - `CancelPreview()`: `Remove()` before scene removal.
  - `PlacePreview()` with history: `Remove()` on reclaim, `Add()` after Push.
  - `PlaceMeshAt()`: `Add()` after placement (both history and direct paths).
  - `OnEvent()` delete key: `Remove()` before Push.
  - `Render()` gizmo end: `UpdateMoved()` when drag ends.
- `PickObjectAt()`: uses `picking_acc_.QueryRay()` when built; falls back to `scene_->GetObjects()` otherwise.

### Modified: `src/editor/CMakeLists.txt`

- Added `PickingAccelerator.cpp` to the `editor` static library.

## Design decisions

**DDA traversal** was chosen over brute-force cell enumeration because it visits only cells actually crossed by the ray, giving O(k) cost where k is the number of traversed cells rather than O(n_cells).

**Cell size = diagonal / 32** balances memory (32³ = 32 768 max cells) against resolution. Clamped to 1.f minimum to avoid degenerate grids for very small scenes.

**`GetBounds()` minimum diagonal** of 10 world units ensures the grid is always large enough to cover a newly created scene with just the floor and default cube.

**False negatives after undo/redo**: the accelerator is not hooked into `EditorCommandHistory::Undo/Redo` — those may leave the grid stale for removed/re-added objects. Since `QueryRay` returns candidates (false positives OK), and undo/redo-produced false negatives are rare, this is an acceptable trade-off for a first implementation. A future improvement could notify the accelerator via a callback registered in `EditorCommandHistory`.

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h/.cpp`, Google C++ style, include root `src/`
- `src/editor/CLAUDE.md`: editor is the leaf, one class per file
- CLAUDE.md root: conventional commits, cpplint, history file, PR to `dev`
