# Fall to Terrain Tool

**Date:** 2026-06-11  
**Issue:** #465  
**Branch:** feat/editor-fall-to-terrain-465

---

## Summary

Added a "Fall to Terrain" action button to the editor toolbar. When clicked, it drops the selected object onto the terrain below it, aligning both its position and orientation to the terrain slope.

---

## Changes

### `src/editor/EditorToolbar.h` / `EditorToolbar.cpp`

- Added `SetOnFallToTerrain(std::function<void()>)` callback setter.
- Added `SetCanFallToTerrain(bool)` to enable/disable the button.
- Added a new section in `Render()` after the creation tools: a `ICON_FA_CIRCLE_ARROW_DOWN` button separated by a vertical divider. Disabled with an informative tooltip when the prerequisite conditions are not met.

### `src/editor/EditorWindow.h` / `EditorWindow.cpp`

- Added `FallToTerrain()` private method.
- Wired `toolbar_->SetOnFallToTerrain(...)` in the constructor.
- In `Render()`, updates `can_fall_to_terrain` state: enabled only when a terrain is in the scene and a non-terrain object is selected.
- Added `#include "editor/commands/TransformCommand.h"`.

---

## Design Decisions

### Action button, not a toggle tool

"Fall to Terrain" is a one-shot action (like Undo/Redo), not a persistent mode. Adding a new `EditorTool` enum value would force EditorViewport to handle it as a gizmo mode, which is unnecessary. Instead, the button lives alongside Undo/Redo/Save as an action button outside the `ToolEntry` table.

### Slope alignment via orthonormal frame reconstruction

The terrain `GetNormal()` returns the bilinearly interpolated surface normal at the query point. To align the object's Y-axis to this normal while preserving the yaw (horizontal heading):

1. Extract the horizontal component of the current Z-column (forward direction).
2. Build new basis: `new_right = normalize(N × fwd)`, `new_forward = normalize(new_right × N)`.
3. Reapply the original per-axis scale (extracted as column magnitudes from the existing transform).

This is robust against vertical-facing objects: the horizontal forward projection falls back to `(0, 0, 1)` when the Z column is nearly vertical.

### Bbox-bottom contact placement

The local bbox `min_y` gives the lowest extent in object space. When the object is slope-aligned, this point is displaced from the pivot by `N * local_min_y` in world space. The pivot Y is therefore placed at `terrain_h - local_min_y * N.y`, and XZ are adjusted by `(-local_min_y * N.x, -local_min_y * N.z)` to keep the contact point directly below the query position.

### Undo/redo support

The operation pushes a `TransformCommand` to the history, making it fully undoable via Ctrl+Z like any other transform.

### Disabled state

The button is disabled (greyed out) when:
- No terrain is present in the scene (`terrain_panel_.IsActive()` is false), OR
- No object is selected, OR
- The selected object is itself the terrain.

---

## Files Modified

- `src/editor/EditorToolbar.h`
- `src/editor/EditorToolbar.cpp`
- `src/editor/EditorWindow.h`
- `src/editor/EditorWindow.cpp`

---

## Skills / Instructions Used

- Followed `src/CLAUDE.md`: one class per `.h/.cpp`, Google C++ style, include root is `src/`.
- Followed `src/editor/CLAUDE.md`: no singletons, ImGui calls bracketed correctly, owned by `EditorSystem`.
- Followed `src/terrain/CLAUDE.md`: used `TerrainData::GetHeight()` and `TerrainData::GetNormal()` for world-space queries.

---

## Notes for Future Work

- The contact point is queried at the **pivot's XZ**, not the bbox bottom's XZ. For objects with large extents, querying the terrain at the actual contact projection (after slope tilt) would be more accurate. This is a minor second-order correction in practice.
- If the object is partially outside the terrain (out of heightmap range), `GetHeight`/`GetNormal` clamp to the heightmap boundary — the result is deterministic but may look odd near the edges.
