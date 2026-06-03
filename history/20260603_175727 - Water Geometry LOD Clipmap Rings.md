# Water Geometry LOD — Clipmap Rings

**Branch:** `feat/water-geometry-lod-385`  
**Issue:** #385  
**Date:** 2026-06-03

## Summary

Replaced the monolithic static water grid with three camera-snapped **clipmap LOD ring meshes**. The rings are rebuilt on the GPU (via dynamic VB/IB `Fill()`) only when the camera moves by at least half a cell width from the last snap position. This reduces vertex count dramatically for large terrains by concentrating geometry density near the camera.

## LOD Ring Specification

| Level | Cell size | Inner radius | Outer radius |
|---|---|---|---|
| LOD 0 (near) | 10 m | 0 m (full disc) | 200 m |
| LOD 1 (mid)  | 20 m | 200 m           | 500 m |
| LOD 2 (far)  | 40 m | 500 m           | terrain margin |

LOD 2's outer radius is derived from terrain dimensions at `Build()` time (`terrain_half × 1.2`), falling back to `max(1000 m, grid_size × 10 m / 2)` when no terrain dimensions are provided.

## Key Design Decisions

### Vertex coordinates are world-space absolute
Vertices store absolute world XZ positions (Y = 0). The Gerstner displacement in the vertex shader is applied in world space using `in_position.xz`, so no additional transform is needed. UV is set to `(wx, wz)` matching what the shader already does (`v_uv = in_position.xz`).

### Per-ring tile frustum culling
Each `LodRing` owns a `std::vector<TileInfo>` rebuilt alongside the geometry in `BuildRingGeometry()`. Tiles with no active quads (after radial filtering) are skipped entirely. The 2D frustum projection from issue #382 applies to each ring's tile list in `Render()`.

### Radial inclusion criterion
Only quads whose XZ centre falls in `[inner_radius², outer_radius²]` distance² from the snap position are emitted. This produces a clean annular shape with no overlap between rings.

### Snap threshold
A ring geometry rebuild is triggered when the snap position moves `≥ cell_size × 0.5` from `last_snap`. This is a standard clipmap heuristic that prevents continuous rebuilds during slow camera movement while keeping seams invisible at normal frame rates.

### Pre-allocation strategy
VB/IB are pre-allocated at `Build()` time with worst-case counts for the bounding square of each ring (`ceil(2 × outer_radius / cell_size)²`). `kDynamic` usage enables subsequent `Fill()` calls without re-allocation.

### `BuildRingGeometry` made `static`
The function only touches the `LodRing` passed by reference and its nested `TileInfo` type — no instance members are accessed. Making it `static` surfaced by cppcheck and applied.

## Files Changed

- `src/environment/WaterRenderer.h`
  - Removed: `grid_vb_`, `grid_ib_`, `num_indices_`, `tiles_` members and `BuildMesh()` method.
  - Added: `LodRing` struct (with embedded `tiles`), `std::array<LodRing, 3> lod_rings_`, `static BuildRingGeometry()`.
  - `TileInfo` kept as private nested struct (unchanged semantics).

- `src/environment/WaterRenderer.cpp`
  - Replaced `BuildMesh()` with ring pre-allocation in `Build()`.
  - Added `BuildRingGeometry()`: radial quad filtering + tile-major index emission.
  - Updated `Render()`: per-ring snap/rebuild loop + per-ring tile frustum culling.
  - Updated `Reset()`: clears all ring resources.

## Interaction with Previous Issues

- **#382 (tile frustum culling):** Preserved. Each ring now gets its own per-tile culling loop, replacing the single monolithic tile list.
- **#384 (fragment distance LOD):** Unaffected. Shader-side feature gating by fragment distance is orthogonal to geometry topology.

## Skills / Instructions Used

- `src/CLAUDE.md`: one class per .h/.cpp, Google C++ style, include root = `src/`.
- `src/environment/CLAUDE.md`: no dependency on `renderer` module; structs with no methods may be header-only.
- `CLAUDE.md` (root): checkout dev first, conventional commits, history file, PR to `dev`.

## Output to Keep in Mind

- The `TileInfo` struct inside `LodRing` could be extracted to a shared header if other systems need similar per-tile index ranges in the future.
- LOD 2 outer radius is computed once at `Build()` time; if `terrain_world_width/height` change at runtime, `Build()` must be called again to update it.
- The snap threshold of `0.5 × cell_size` was chosen as a standard clipmap value — if popping artefacts appear on very slow camera movement, the threshold could be reduced to `0.25 × cell_size`.
