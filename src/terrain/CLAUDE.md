# CLAUDE.md — terrain module

## Role

The `terrain` module owns all terrain-related data and logic: heightmap storage,
height/normal queries, LOD management, and eventual GPU resource creation.

## Dependency rules

- **Allowed**: `core`, `abstract`
- **Forbidden**: `gldevices` (no OpenGL/platform headers), `editor`, `renderer`
  (terrain is a pure-data/abstract layer; higher-level modules depend on it,
  not the other way around)

## Current classes

| Class | File | Purpose |
|-------|------|---------|
| `TerrainData` | `TerrainData.h/.cpp` | CPU heightmap storage; height and normal queries |
| `TerrainPatch` | `TerrainPatch.h` | Plain data struct: LOD level, grid position, morph factor |
| `CDLODQuadTree` | `CDLODQuadTree.h/.cpp` | CDLOD quadtree: Build from terrain + Select visible patches |
| `TerrainPatchMesh`   | `TerrainPatchMesh.h/.cpp`   | Shared flat NxN grid VBO/IBO reused by all patches at all LOD levels |
| `TerrainPatchInfos`  | `TerrainPatchInfos.h`       | Per-patch constant buffer struct (slot 6) — 48 bytes / 3 float4s     |
| `TerrainRenderer`    | `TerrainRenderer.h/.cpp`    | Singleton G-buffer renderer: heightmap upload, quadtree, draw loop   |

## Conventions

- Follow `src/CLAUDE.md`: one class per `.h/.cpp`, Google C++ style, include
  root is `src/`.
- Heightmap samples are `uint16_t`, row-major, width × height elements.
- World-space coordinate system: Y-up, X-right, Z-forward.
- `GetHeight` / `GetNormal` take world-space (x, z) in metres.
