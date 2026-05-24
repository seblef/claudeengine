# MeshTemplate — CPU-side vertex positions for ray picking

**Date:** 2026-05-24
**Issue:** #268
**Branch:** feat/meshtemplate-cpu-positions-268

## Summary

Added CPU-side retention of vertex positions and triangle indices after GPU upload so that future ray-triangle intersection code (EditorViewport precise mesh selection) can work without re-reading disk.

## Changes

### `src/renderer/MeshLoader.h` / `MeshLoader.cpp`

- Added `CPUMeshData` struct: `positions` (one `Vec3f` per vertex) and `indices` (one `uint32_t` per index).
- Added `MeshLoadResult` struct bundling `geometry` (GPU) and `cpu` (CPU).
- Added `MeshLoader::Load()` which returns `std::optional<MeshLoadResult>` — builds GPU geometry via `MakeGeometry()` then copies positions from `lod.vertices` and moves indices.
- `LoadGeometry()` is now a thin wrapper calling `Load()` and returning only the geometry pointer; all existing callers remain unaffected.

### `src/game/MeshTemplate.h` / `MeshTemplate.cpp`

- File-backed constructor now calls `MeshLoader::Load()` instead of `LoadGeometry()` and moves the `CPUMeshData` into `cpu_positions_` / `cpu_indices_` members.
- Procedural constructor (`cube`/`sphere` previews) leaves both vectors empty — callers must guard with `GetCPUPositions().empty()`.
- Exposed accessors: `GetCPUPositions()` and `GetCPUIndices()`.

## Decisions

- Positions only (no normals/UVs) are stored CPU-side to minimise memory overhead while satisfying ray-picking needs.
- `std::optional` on `Load()` is the idiomatic way to express "may fail" without heap allocation; `LoadGeometry()` keeps its `unique_ptr` return type so no callers change.
- The 16-bit index limit check is kept inside `MakeGeometry()` (GPU path); CPU indices remain `uint32_t` as that is what the importers produce.

## Output / Notes for Next Features

- `MeshTemplate::GetCPUPositions()` + `GetCPUIndices()` are ready for use by EditorViewport ray-triangle intersection.
- Procedural templates (ids starting with `__proc_`) have empty CPU data — always check before iterating.
- Memory cost: ~12 bytes per vertex for positions + 4 bytes per index, held for the lifetime of each `MeshTemplate` in the resource cache.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp`, Google style, include root is `src/`.
- `src/game/CLAUDE.md`: `MeshTemplate` is a reference-counted resource, obtained via `GetOrLoad()`.
- Conventional commits message format.
- `cpplint` clean before commit.
