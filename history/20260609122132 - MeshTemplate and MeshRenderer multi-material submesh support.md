# MeshTemplate and MeshRenderer multi-material submesh support

**Issue:** #423
**Branch:** `feat/renderer-mesh-template-multi-material`
**Date:** 2026-06-09

## Summary

Extended `MeshTemplate` and `MeshRenderer` to support one `GameMaterial` per
submesh slot, enabling meshes with multiple materials to render each submesh
with its own material in a dedicated draw call.

## Changes

### `renderer/Mesh.h` / `renderer/Mesh.cpp`

- Added a `std::vector<Material*> materials_` field replacing the single
  `Material* material_`.
- Added a multi-material constructor:
  `Mesh(GeometryData*, std::vector<Material*>)`.
- Kept the original single-material constructor for backward compat; it
  initialises `materials_` with one entry.
- Added `GetMaterial(int slot)`, `SetMaterial(int slot, Material*)`, and
  `GetMaterialCount()`.
- `GetMaterial()` and `SetMaterial(Material*)` (no slot) still target slot 0.

### `game/MeshTemplate.h` / `game/MeshTemplate.cpp`

- Replaced `GameMaterial* material_` with `std::vector<GameMaterial*> materials_`.
- Added `SetMaterial(int slot, GameMaterial*)` and `GetMaterial(int slot)`.
- Added `GetMaterialCount()`.
- `SetMaterial(GameMaterial*)` and `GetMaterial()` remain as slot-0 shorthands
  (backward-compatible with all editor callers such as `MaterialAssignCommand`
  and `MaterialEditorWindow`).
- Added private helper `InitMaterials()` to centralise the material population
  logic during file-backed construction:
  - If `GetSubMeshCount() > 0`, allocates one `GameMaterial` per submesh,
    loading it by name via `GameMaterial::GetOrLoad()` (or creating a default
    if `material_name` is empty); an optional `override_mat` replaces slot 0.
  - If `GetSubMeshCount() == 0` (legacy), falls back to the original
    single-material path.

### `renderer/MeshRenderer.cpp`

- `Render()` now checks `geo->GetSubMeshCount()`:
  - **0** → existing fast path (single `RenderIndexed` for the full index
    range, unchanged).
  - **> 0** → iterates submeshes; for each slot `i`, binds `mesh->GetMaterial(i)`
    and calls `RenderIndexed(sub.index_count, sub.index_offset)`.
- `RenderEmissive()` applies the same branching; each submesh is tested
  independently for `IsEmissive()`.
- `RenderDepth()` and `RenderDepthCube()` are unchanged (depth pass is
  material-agnostic).
- Switched internal geometry-change tracking from `Mesh*` to `GeometryData*`
  so the geometry is re-bound only when the actual GPU buffers change, which is
  correct for both single- and multi-material instances sharing a geometry.

## Design decisions

- **Backward compatibility is preserved at every layer.** All callers that use
  `SetMaterial(mat)` / `GetMaterial()` (no slot) continue to work without change.
- **Depth passes ignore submesh materials.** Shadow rendering only needs
  geometry; splitting depth draw calls per submesh would add overhead with no
  benefit, so the full geometry is still drawn in one call.
- **Sorting** in `PrepareRender()` continues to use slot-0 material as the
  primary key. This groups single-material instances optimally; for multi-
  material instances it at least groups them by first material, minimising
  redundant state changes on slot 0.
- **Material loading from file.** When a mesh file carries submesh ranges with
  non-empty `material_name` fields, `MeshTemplate` loads them automatically via
  `GameMaterial::GetOrLoad()`. This means a multi-material FBX is self-contained
  — no external configuration needed.

## Output to keep in mind

- `MeshTemplate::SetMaterial(int slot, ...)` is now the canonical way to
  override a material for a specific submesh. Editor commands that do per-slot
  assignment should call this overload.
- The `Mesh::GetMaterialCount()` accessor can be used by any renderer or tool
  that needs to iterate materials without touching the geometry.
- `RenderEmissive()` now correctly handles multi-material meshes: only the
  submeshes with an emissive material are drawn in the emissive pass.

## Skills and CLAUDE.md instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, `src/` as include root
- `src/game/CLAUDE.md`: dependency order (`game → renderer → abstract → core`),
  no platform headers in game
