# Flatten mesh importers: one geometry per file

**GitHub issue**: #202  
**Branch**: feat/flatten-mesh-importers  
**Milestone**: Game materials

## Summary

Simplified the mesh data model so that each source file yields exactly one flat geometry. Materials are now a game-level concept; importers handle geometry only.

## Changes

### `src/mesh/MeshData.h`
Replaced `vector<SubmeshData> submeshes` with a single `LodData lod`. The top-level mesh asset now holds exactly one level-of-detail, with no submesh dimension.

### `src/mesh/SubmeshData.h`
**Deleted.** The concept of submeshes no longer exists in the data model.

### `src/mesh/ObjImporter.cpp`
All OBJ groups are merged into `mesh->lod` in a single pass. The `MaterialSlotName()` helper is removed. Welding, normal recomputation, tangent computation, and AABB are applied once on the merged result.

### `src/mesh/FbxImporter.cpp`
All FBX mesh objects and all their faces are iterated and merged into `mesh->lod`. Material parts are ignored. The `MaterialSlotName()` helper is removed. Per-mesh `has_normals` is tracked globally (`all_have_normals`); `ComputeNormals` is called if any mesh lacked normals.

### `src/mesh/EmeshWriter.h` / `EmeshWriter.cpp`
New binary layout (version 2): `magic[4] | version u32 | vertex_count u32 | index_count u32 | aabb_min float[3] | aabb_max float[3] | vertices[] | indices[]`. Removed submesh count, LOD count, and per-submesh material slot name.

### `src/mesh/EmeshReader.h` / `EmeshReader.cpp`
Updated to read the version-2 layout. Version check is strict (only v2 accepted). Reads directly into `mesh->lod`.

### `src/renderer/MeshLoader.cpp`
Updated to use `data.lod` directly. Removed material-slot lookup logic (materials are game-level). A default `Material` is assigned for now; the material assignment will move to `game::GameMaterial` in issue #204.

### `tests/mesh/ObjImporterTest.cpp` / `FbxImporterTest.cpp`
Updated to access `mesh_.lod` instead of `mesh_.submeshes[0].lods[0]`. The `OneLod` test (which checked `submeshes.size() == 1`) is removed since that dimension no longer exists.

## Decisions

- **Version bump to 2**: The .emesh binary format is incompatible. Any previously-serialised .emesh files must be re-exported. This is acceptable in a dev milestone context.
- **`all_have_normals` in FbxImporter**: When merging multiple FBX mesh objects, the safest policy is to recompute normals if any mesh lacked them. Mixed normal availability would produce incorrect shading.
- **MeshLoader keeps `RenderableMesh` return type**: Issue #203 will change MeshLoader to return `unique_ptr<GeometryData>` and remove `RenderableMesh`. Changing the return type here would bloat this PR with unrelated edits.

## Output to keep in mind

- `.emesh` files are now format version 2. Old v1 files will be rejected at runtime with `EmeshReader: unsupported version`.
- `mesh::SubmeshData` is gone. Any future code must not reference it.
- `renderer::MeshLoader::Load()` still returns `unique_ptr<RenderableMesh>` with a default (empty) material; this is temporary until Issue #203.

## Skills / CLAUDE.md instructions used

- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair, Google C++ style, include root is `src/`.
- `src/mesh/` (inferred from existing code): AABB computed after weld, tangents always recomputed.
- Project `CLAUDE.md`: conventional commits, PR to `dev`, history file required.
