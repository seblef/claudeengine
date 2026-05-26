# TerrainEditorPanel — Sculpt Tools (Issue #295)

**Date:** 2026-05-26  
**Branch:** feat/terrain-sculpt-panel-295  
**Closes:** #295

---

## Overview

Adds in-engine terrain sculpting to the editor via a new dockable `TerrainEditorPanel` ImGui panel. Includes four brush tools (Raise, Lower, Smooth, Flatten), configurable radius / strength / falloff, viewport raycast hit detection, and full undo/redo via `SculptBrushCommand`.

---

## Changes

### `src/terrain/TerrainData.h/.cpp`
- Added `GetSample(tx, tz)` — bounds-clamped uint16_t read.
- Added `SetSample(tx, tz, value)` — direct write for sculpt (caller bounds-checked).
- Added `HeightToSample(h)` — converts world-space height to uint16_t, clamped to [0, 65535].

### `src/terrain/TerrainRenderer.h/.cpp`
- Added `UpdateHeightmapTile(tx, tz, w, h, data)` — extracts a contiguous sub-tile from `TerrainData` and calls `RawTexture::UpdateRegion()`, mirroring the pattern used by `TerrainNormalMap::UploadTile()`.

### `src/editor/commands/SculptBrushCommand.h/.cpp` *(new)*
- `EditorCommand` subclass storing pre/post uint16_t snapshots of the affected region.
- `Execute()` / `Undo()` both call `ApplySnapshot()` which writes samples back to `TerrainData`, calls `TerrainNormalMap::UploadTile()`, and calls `TerrainRenderer::UpdateHeightmapTile()`.

### `src/editor/TerrainEditorPanel.h/.cpp` *(new)*
- Dockable ImGui panel with:
  - Tool buttons: **Raise | Lower | Smooth | Flatten**
  - `SliderFloat` for radius (0.5–200 m) and strength (0.01–1.0)
  - Radio buttons for **Linear** / **Smooth** falloff
- `OnBrushAt(wx, wz, first_touch, dt)`: called each frame LMB is held in the viewport.
  - On first touch: snapshots the initial brush AABB into `stroke_pre_snapshot_`.
  - On subsequent touches: `EnsureRegionCovers()` expands the snapshot region if the brush has moved outside the initially tracked area — new texels are snapshotted from current (unmodified) data before the brush touches them.
  - `ApplyBrushFootprint()` applies the tool to all texels within `radius_`, then calls `TerrainRenderer::UpdateHeightmapTile()` and `TerrainNormalMap::UploadTile()` for the brush footprint.
- `OnBrushEnd()`: takes a post-snapshot and pushes a `SculptBrushCommand` to history (no-op when nothing changed).

### `src/editor/EditorViewport.h/.cpp`
- Added `SetTerrainData()`, `SetSculptActive()`, `SetOnSculptBrush()`, `SetOnSculptEnd()`.
- Added `ComputeTerrainHit()`: ray-marches against `TerrainData` (64 steps, 8-step binary search) to find the world-space terrain surface hit; falls back to the y=0 horizontal plane when no terrain data is provided.
- Sculpt block in `Render()`: when `sculpt_active_` and LMB is held (no Alt), fires `on_sculpt_brush_` each frame; fires `on_sculpt_end_` on LMB release.
- Object picking is gated by `!sculpt_active_` to avoid conflicts.

### `src/editor/EditorWindow.h/.cpp`
- Added `TerrainEditorPanel terrain_panel_` and `bool show_terrain_panel_`.
- Added private `WireTerrainPanel()`: retrieves the `GameTerrain` from the scene, calls `terrain_panel_.SetContext()` and wires the viewport sculpt callbacks via lambdas. Called from `CreateTerrain()`, `LoadFromFile()`, and the New Map modal.
- New **Terrain** menu (between Map and Add) with a **Sculpt** toggle item; disabled when no terrain is in the scene.
- Added `FindTerrain()` anonymous-namespace helper.

---

## Design Decisions

### Snapshot expansion
The stroke pre-snapshot grows as the brush moves. New texels are captured from unmodified `TerrainData` immediately before the brush footprint is applied. This guarantees correctness while keeping snapshot memory proportional to the actual stroke area.

### Brush rate (`kBrushRate = 10 m/s`)
`h += strength * w * dt * kBrushRate`. At strength=1, dt=0.016, center weight w=1: 0.16 m/frame ≈ 10 m/s. Practical for a 100 m height range; can be adjusted per project by changing the constant.

### Flatten height
Sampled at the first LMB down point (not the brush center on each frame). This makes the flatten tool deterministic: repeated strokes continue to flatten toward the same reference.

### const_cast in WireTerrainPanel
`GameTerrain::GetData()` returns `const TerrainData&`. The editor needs a mutable pointer to sculpt. Rather than adding a `GetMutableData()` accessor to `GameTerrain` (which would expose mutation to all users), `EditorWindow` uses `const_cast` — safe because the underlying object is non-const.

### Sculpt vs selection mode
When the Terrain > Sculpt panel is open (`show_terrain_panel_ = true`), `viewport_->SetSculptActive(true)` is called each frame, which disables object picking in the viewport. When the panel is closed, sculpt is disabled and picking resumes.

---

## GPU Update Strategy

Each brush stroke fires:
1. `TerrainRenderer::UpdateHeightmapTile()` — uploads the brush AABB to the R16 heightmap texture.
2. `TerrainNormalMap::UploadTile()` — recomputes and uploads the normal map for the same region (automatically inflated by 1 texel for central-difference accuracy).

Undo/redo writes back via `SculptBrushCommand::ApplySnapshot()` which calls both methods for the full stroke region.

---

## Files Used

- `src/editor/CLAUDE.md`
- `src/terrain/CLAUDE.md`
- `src/CLAUDE.md`
- `CLAUDE.md`

## Instructions Followed

- One class per `.h/.cpp` pair
- Google C++ style
- `#include` root is `src/`
- No singleton abuse: `TerrainRenderer::IsInstanced()` guard before calling `Instance()`
- `// cppcheck-suppress unusedStructMember` on all private data members
- cpplint clean
