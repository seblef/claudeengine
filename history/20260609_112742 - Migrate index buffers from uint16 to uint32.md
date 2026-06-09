# Migrate index buffers from uint16 to uint32

**Issue:** #420
**Branch:** feat/index-buffer-uint32

## Summary

`GeometryData` previously created `kUInt16` index buffers and accepted `const uint16_t* indices`, capping any single mesh at 65 535 vertices. `LodData` and the mesh importers already stored indices as `uint32_t`, so `MeshLoader` was performing a silent narrowing conversion through a temporary `idx16` vector. This change removes that cap and converts all geometry creation paths to `kUInt32`.

## Changes

### `src/renderer/GeometryData.h` / `GeometryData.cpp`
- Constructor parameter changed from `const uint16_t* indices` to `const uint32_t* indices`.
- `IndexType::kUInt16` → `IndexType::kUInt32` in the `CreateIndexBuffer` call.
- Updated class-level comments.

### `src/renderer/MeshLoader.cpp`
- Removed the 65 535-vertex guard and the `idx16` narrowing-cast vector entirely.
- `MakeGeometry` now passes `lod.indices.data()` (already `uint32_t*`) directly to `GeometryData`.

### `src/renderer/GeometryUtils.cpp`
- All six index arrays/vectors (`CreateQuad`, `CreateSphere`, `CreateCone`, `CreatePyramid`, `CreatePlaneMesh`, `CreateCubeMesh`) changed from `uint16_t` to `uint32_t`; narrowing `static_cast`s removed or converted to widening casts.

### `src/renderer/FoliageRenderer.cpp` / `TreeRenderer.cpp`
- Billboard quad index arrays changed from `uint16_t` to `uint32_t`.

## Decisions and rationale

- **Terrain patches untouched**: `TerrainPatchMesh` creates its geometry via a separate code path that does not go through `GeometryData`; its patches are bounded to 255 quads/side (well within uint16 range) and the comment already documents this. Changing it would be out of scope.
- **No API surface change on `IndexBuffer`**: `kUInt32` already existed in `abstract::IndexType`; no abstract or GL backend changes were needed.
- **Vertex limit check removed**: The 65 535 check in `MakeGeometry` was the only consumer of the limit. Removing it (rather than updating it to a `uint32_t` ceiling) keeps the code clean — the GPU itself enforces the 4 billion limit.

## Output to keep in mind

- All subsequent submesh/multi-mesh work can now rely on 32-bit indices throughout the renderer.
- The `TerrainPatchMesh` path still uses `uint16_t` internally and passes to `GeometryData` (if it does) — verify that path if terrain geometry is later modified.

## Skills and instructions used

- Project CLAUDE.md: git workflow (branch from dev, conventional commit, PR to dev)
- `src/CLAUDE.md`: one class per file, Google style, no unnecessary comments
