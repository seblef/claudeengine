# Water: View-Frustum Tile Culling

**Branch:** `feat/water-frustum-tile-culling-382`  
**Issue:** #382  
**Date:** 2026-06-03

## Summary

`WaterRenderer` previously submitted the entire water grid in a single `RenderIndexed` call, ignoring the camera direction. At typical angles roughly half the mesh is behind the camera. This change partitions the grid into 16×16-cell tiles and skips tiles outside the view frustum each frame, reducing GPU vertex and rasterisation work by an estimated 30–50%.

## Changes

### `WaterRenderer.h`

- Added private `struct TileInfo { int first_index; int index_count; core::BBox3 aabb; }` to hold per-tile data.
- Added `std::vector<TileInfo> tiles_` member.
- Changed `SetWaterLevel(float)` from an inline setter to a declared method (implemented in `.cpp`) so tile AABBs can be updated in-place.
- Added includes for `core/BBox3.h`.

### `WaterRenderer.cpp`

- Added `constexpr int kTileSize = 16` and `constexpr float kWaveAmplitude = 2.f`.
- **Index buffer now emitted in tile-major order**: outer loops iterate over tiles, inner loops over cells within each tile. This makes each tile's indices a contiguous sub-range of the buffer, enabling a single `RenderIndexed(count, first)` call per visible tile without sub-buffer copies.
- `BuildMesh()` fills `tiles_` alongside the index buffer — `first_index`, `index_count`, and an AABB covering the tile's XZ world extent plus ±2 m on Y for Gerstner wave amplitude.
- `Render()`: removed `[[maybe_unused]]`; constructs `core::ViewFrustum(camera.GetViewProjectionMatrix())` and calls `frustum.ContainsBBox(tile.aabb)` to cull each tile before issuing its draw call.
- `SetWaterLevel()`: updates `water_level_` and patches `min_.y` / `max_.y` on all tile AABBs.
- `Reset()` clears `tiles_`.

## Decisions and Rationale

### Tile-major index order

A tile's cells are not contiguous in the original row-major flat buffer (rows are separated by `grid_size - tile_width` cells). Reordering to tile-major at build time costs nothing at runtime and avoids issuing one draw call per cell row — which could be hundreds of calls for large grids.

### 16×16 cells per tile (160 m × 160 m)

The issue specified this size. At 10 m per cell a 128-grid produces 64 tiles, keeping the per-frame loop overhead negligible while giving enough granularity for effective culling.

### Y margin of ±2 m

Matches the issue specification and is sufficient to encompass Gerstner wave amplitude. This prevents popping at the frustum boundary when waves displace vertices above the flat water plane.

## Output to Keep in Mind

- The index buffer winding and vertex layout are unchanged — only the emission order differs, so no visual regression is expected.
- `num_indices_` is still stored as the total count for potential fallback use, though `Render()` no longer uses it directly.
- `core::ViewFrustum` is constructed per frame on the stack — no heap allocation.

## Skills / CLAUDE.md Instructions Used

- Project CLAUDE.md: conventional commit prefix (`feat`), branch naming, cpplint gate, history file.
- `src/CLAUDE.md`: one class per file, project-relative includes, Google C++ style.
- `src/environment/CLAUDE.md`: environment module dependency rules (no dependency on `renderer`).
- Feedback memory: `cppcheck-suppress unusedStructMember` for members used only in `.cpp`.
