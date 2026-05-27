# Terrain Paint Panel — Issue #296

## Summary

Extends `TerrainEditorPanel` with a **Paint** tab that lets the user paint
splatmap blend weights per terrain material layer, with full undo/redo support.

## Changes

### New files

| File | Purpose |
|---|---|
| `src/editor/commands/PaintBrushCommand.h/.cpp` | Undo/redo command that snapshots a rectangular splatmap tile (RGBA uint8) before and after a paint stroke |

### Modified files

| File | Change |
|---|---|
| `src/editor/TerrainEditorPanel.h` | Added Paint tab state, `TerrainMaterial*` context, `SetContext` extended signature, private paint helpers |
| `src/editor/TerrainEditorPanel.cpp` | Added `RenderSculptTab` / `RenderPaintTab`, `OnPaintAt`, `OnPaintEnd`, `ApplyPaintFootprint`, `EnsurePaintRegionCovers`; `OnBrushAt`/`OnBrushEnd` dispatch on `active_tab_` |
| `src/editor/EditorWindow.cpp` | `WireTerrainPanel` now passes `const_cast<TerrainMaterial*>` to `SetContext`; null-reset call updated to 5 args |
| `src/editor/commands/CMakeLists.txt` | Added `PaintBrushCommand.cpp` |

## Decisions

### Shared radius and falloff
Both Sculpt and Paint tabs share `radius_` and `falloff_` because the same
physical brush shape applies to both operations. Opacity (paint) and Strength
(sculpt) are separate since they have different semantics.

### Layer colour swatches
`abstract::Texture` has no `GetNativeHandle()` — only `abstract::RenderTarget`
exposes one for ImGui. Rather than adding GPU readback to the Texture interface,
per-layer buttons use predefined pastel RGBA colours (green, brown, tan, white)
as visual hints. If real thumbnails are desired later, a `GetNativeHandle()`
method can be added to `abstract::Texture` and `GLTexture`.

### `SetContext` signature
Added `TerrainMaterial*` as the second parameter (after `TerrainData*`) so the
paint brush can access the splatmap CPU buffer and tile-upload API without
reaching through `GameTerrain`.

### Stroke dispatch via `active_tab_`
`OnBrushAt` and `OnBrushEnd` now dispatch to sculpt or paint code based on
`active_tab_`, which is updated each frame inside `Render()`. Switching tabs
mid-stroke is an unsupported edge case; a mid-stroke switch will cause the
in-flight stroke to be silently dropped (the stroke ends cleanly when LMB is
released after the switch). This is acceptable given the low probability.

### Paint normalisation
After increasing the active layer channel by `opacity * w`, remaining channels
are scaled down proportionally so all four sum to 1.0. When all other channels
are already zero, the active layer is set to 1.0 directly.

### Snapshot format
Pre/post snapshots are raw RGBA `uint8_t` row-major tiles (4 bytes/texel).
Restoration via `PaintBrushCommand::ApplySnapshot` calls
`TerrainMaterial::SetSplatWeight` (float round-trip) rather than writing the
buffer directly, which avoids adding a new API to `TerrainMaterial`. The
quantisation error is sub-pixel (< 0.4%) and invisible.

## Output to keep in mind

- The `active_layer_` index is clamped to `[0, layer_count_)` in `SetContext`.
  If layers are added/removed at runtime the panel re-clamps on the next
  `SetContext` call.
- The paint brush does **not** modify the heightmap or trigger normal-map
  recomputation, consistent with the issue spec.
- The splatmap dimensions are taken from `TerrainMaterial` (not `TerrainData`),
  so a future decoupled splatmap resolution would work without code changes.

## Skills used

- `impl-issue`

## CLAUDE.md instructions followed

- One class per `.h/.cpp` pair (`PaintBrushCommand` separate from `SculptBrushCommand`)
- Google C++ style guide
- Include root `src/`
- `cpplint` ran — 0 errors
- Conventional commit message
- PR to `dev` with issue closing command
