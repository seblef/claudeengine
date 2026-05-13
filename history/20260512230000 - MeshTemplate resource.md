# MeshTemplate resource

**Date:** 2026-05-12
**Issue:** #127
**PR:** #136
**Branch:** feat/game-mesh-template

## Changes

- `src/game/MeshTemplate.h` — class declaration with two constructor/GetOrLoad overloads
- `src/game/MeshTemplate.cpp` — implementation including YAML material loading
- `src/game/CMakeLists.txt` — added `MeshTemplate.cpp`

## Decisions

**Delegating constructor for YAML path overload:** The `(mesh_path, video, material_file_path)` constructor delegates to `(mesh_path, video, MaterialDesc)` by calling `LoadMaterialDesc()` in its member initialiser. This avoids duplicating the `MeshLoader::Load` call and ensures `initialized_` handling is in one place.

**YAML parsing only covers textures:** The existing `data/materials/CLAUDE.md` defines a format with only a `textures` section. Color properties (diffuse_color, shininess, etc.) are not in the YAML format and are left at `MaterialDesc` defaults. This matches the existing demo material file.

**Parse failure falls back to default MaterialDesc:** If the YAML file is missing or malformed, `LoadMaterialDesc()` logs an error and returns a default `MaterialDesc`. The mesh still loads with default materials rather than failing entirely.

**Resource map key is mesh_path, not material_path:** Two callers loading the same mesh with different materials get the same cached `MeshTemplate`. The first load wins. This is intentional — the resource deduplication is per-geometry, not per-material.

## Output to keep in mind

- Callers must call `Release()` when done. `GameMesh` does this in its destructor.
- `GetOrLoad` with the material_file_path overload still checks the cache by mesh_path first, ignoring the material file if the mesh is already cached.
- The YAML format may be extended with color properties in a future milestone.

## Skills / instructions used

- `data/materials/CLAUDE.md`: YAML material format (textures only, all optional)
- `src/CLAUDE.md`: one class per .h/.cpp; include root is `src/`
- Root `CLAUDE.md`: git workflow, conventional commits
