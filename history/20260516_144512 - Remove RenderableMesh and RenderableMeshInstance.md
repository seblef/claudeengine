# Remove RenderableMesh/RenderableMeshInstance; MeshLoader returns GeometryData

**GitHub issue**: #203  
**Branch**: feat/remove-renderable-mesh  
**Milestone**: Game materials

## Summary

Removed the multi-submesh renderer abstractions (`RenderableMesh`, `RenderableMeshInstance`) that were superseded by the flat one-geometry-per-mesh design from Issue #202. `MeshLoader` is now geometry-only (`LoadGeometry()`). `CreatePlaneMesh`/`CreateCubeMesh` return `GeometryData` directly. `MeshTemplate` owns geometry + material + the `Mesh` pairing; `GameMesh` uses a single `MeshInstance`.

## Changes

### Deleted files
- `src/renderer/RenderableMesh.h` / `RenderableMesh.cpp`
- `src/renderer/RenderableMeshInstance.h` / `RenderableMeshInstance.cpp`

### `src/renderer/MeshLoader.h` / `MeshLoader.cpp`
Renamed `Load()` → `LoadGeometry()`, return type changed from `unique_ptr<RenderableMesh>` to `unique_ptr<GeometryData>`. Removed Material creation and `AddSubmesh` logic entirely.

### `src/renderer/GeometryUtils.h` / `GeometryUtils.cpp`
- `CreatePlaneMesh(video, half_size)` — dropped `Material*` parameter, returns `unique_ptr<GeometryData>`.
- `CreateCubeMesh(video)` — dropped `Material*` parameter, returns `unique_ptr<GeometryData>`.
- Removed `#include "renderer/RenderableMesh.h"` and `#include "renderer/Material.h"` from both files.

### `src/renderer/CMakeLists.txt`
Removed `RenderableMesh.cpp` and `RenderableMeshInstance.cpp` from the source list.

### `src/game/MeshTemplate.h` / `MeshTemplate.cpp`
- Members changed from `unique_ptr<RenderableMesh>` to `unique_ptr<GeometryData>` + `unique_ptr<Material>` + `unique_ptr<Mesh>`.
- File constructor: calls `MeshLoader::LoadGeometry()`, creates a default `Material(video)`, wraps both in a `Mesh`.
- Procedural constructor: now takes `(unique_ptr<GeometryData>, unique_ptr<Material>)` instead of a raw `RenderableMesh*`.
- `GetRenderableMesh()` removed; replaced by `GetMesh() → Mesh*` and `GetLocalBBox() → const BBox3&`.

### `src/game/GameMesh.h` / `GameMesh.cpp`
- `instance_` changed from `unique_ptr<RenderableMeshInstance>` to `unique_ptr<MeshInstance>`.
- Constructor uses `tmpl->GetLocalBBox()` (from geometry) instead of `tmpl->GetRenderableMesh()->GetLocalBBox()`.

### `src/editor/EditorScene.cpp`
Updated procedural mesh template construction to pass `unique_ptr<GeometryData>` and `unique_ptr<Material>` to `MeshTemplate`. Material raw pointers are still stored in `materials_` (non-owning, valid because `MeshTemplate` is ref-counted and outlives the scene).

### `src/app/main.cpp`
- Floor plane construction updated to use new `CreatePlaneMesh` signature and `MeshTemplate` procedural constructor.
- Null-checks updated from `GetRenderableMesh()` to `GetMesh()`.
- Removed `renderer::RenderableMesh*` typed local variable.

## Decisions

- **MeshTemplate owns the default Material**: Until Issue #204 (GameMaterial), material assignment stays in MeshTemplate. A default (untextured) Material is created on construction. GameMaterial will replace this.
- **Procedural constructor signature**: Changed from `RenderableMesh*` to `(unique_ptr<GeometryData>, unique_ptr<Material>)` to make ownership explicit and avoid raw-pointer heap allocation at call sites.
- **`GetLocalBBox()` on MeshTemplate**: Added because callers (GameMesh constructor) need the bbox to initialise `GameObject`. Previously it came from `RenderableMesh::GetLocalBBox()`; now it delegates to `geometry_->GetBBox()`.

## Output to keep in mind

- `renderer::RenderableMesh` and `renderer::RenderableMeshInstance` are gone. Any future code must not reference them.
- `MeshLoader::Load()` is gone; use `MeshLoader::LoadGeometry()`.
- `CreatePlaneMesh` / `CreateCubeMesh` no longer accept a `Material*`; callers must pass a material separately to `MeshTemplate`.
- `MeshTemplate` now exposes `GetMesh()` and `GetLocalBBox()` instead of `GetRenderableMesh()`.
- Issue #204 will replace the default `Material` in `MeshTemplate` with a proper `game::GameMaterial`.

## Skills / CLAUDE.md instructions used

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, Google C++ style, include root is `src/`.
- `src/game/CLAUDE.md`: one class per `.h`/`.cpp` pair, no platform/OpenGL headers.
- Project `CLAUDE.md`: conventional commits, PR to `dev`, history file required.
