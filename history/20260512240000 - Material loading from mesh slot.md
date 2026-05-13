# Material loading from mesh material_slot

**Date:** 2026-05-12
**Issue:** #137
**PR:** #138
**Branch:** feat/material-loading-from-slot

## Changes

- `src/renderer/MeshLoader.h` — removed `MaterialDesc` parameter from `Load()`; updated doc comment with slot convention
- `src/renderer/MeshLoader.cpp` — `FromMeshData` creates `Material(slot + ".yaml", video)` for non-empty slots, `Material(video)` for empty slots
- `src/game/MeshTemplate.h` — single constructor and `GetOrLoad` overload; no material params
- `src/game/MeshTemplate.cpp` — removed `LoadMaterialDesc`, simplified drastically
- `src/app/main.cpp` — removed `MaterialDesc` include and inline descriptor objects

## Decisions

**Appending `.yaml` in MeshLoader, not in Material constructor:** `Material(name, video)` builds `data/materials/<name>` verbatim without adding any extension. Adding `.yaml` at the MeshLoader call site keeps the Material API unchanged and makes the convention explicit in the one place that applies it.

**Empty material_slot → default material:** Both OBJ (demo.obj has no material assignments) and FBX importers may produce empty slots. An empty slot gracefully falls back to `Material(video)` rather than trying to load a non-existent file.

**MeshTemplate loses the YAML overload:** The YAML material loading that was added in issue #127 is no longer needed — the mesh loader handles it. This removed ~40 lines of `LoadMaterialDesc` YAML parsing code from the game layer.

## Convention to document

`material_slot` values should be set by mesh files to match paths relative to `data/materials/` without the `.yaml` extension. For example, a submesh with `material_slot = "characters/hero"` requires `data/materials/characters/hero.yaml` to exist at runtime.

## Output to keep in mind

- Any mesh asset that needs custom materials must embed the correct material slot name in the mesh file (OBJ usemtl name, FBX material name, emesh binary).
- The demo meshes (demo.obj, demo.fbx) have empty material slots → they render with the default material. To get custom colors/textures on the demo, update the mesh files' material references.

## Skills / instructions used

- `src/CLAUDE.md`: include root is `src/`; Google C++ style
- Root `CLAUDE.md`: git workflow, conventional commits
