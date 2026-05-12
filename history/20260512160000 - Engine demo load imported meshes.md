# Engine demo: load and render imported meshes

**Date:** 2026-05-12  
**Issue:** #115 — [feat] Engine demo: load and render imported meshes  
**Branch:** feat/mesh-demo-load  
**Milestone:** Mesh Pipeline v1

---

## Summary

Validates the OBJ and FBX importers end-to-end by loading `data/meshes/demo.obj` (via `ObjImporter`) and `data/meshes/demo.fbx` (via `FbxImporter`) at engine startup and displaying both meshes in the viewport alongside the existing procedural scene.

---

## Changes

### `data/meshes/demo.obj` (new)
Hand-crafted unit cube (8 positions, per-face normals, 4 UV coords), triangulated into 12 triangles. Used as the OBJ demo asset.

### `CMakeLists.txt`
Added a `configure_file` block (after the ufbx setup, where `ufbx_SOURCE_DIR` is defined) that copies `blender_279_default_6100_ascii.fbx` from the ufbx test suite to `data/meshes/demo.fbx` at CMake configure time. This avoids committing a binary file to the repository.

### `src/app/CMakeLists.txt`
Added `mesh` to the `claude_engine` target's link libraries so the app can call `ObjImporter` and `FbxImporter`.

### `src/app/main.cpp`
- **New includes:** `mesh/FbxImporter.h`, `mesh/LodData.h`, `mesh/MeshData.h`, `mesh/ObjImporter.h`.
- **`MakeGeometry` helper (anonymous namespace):** Converts a `mesh::LodData` to a `renderer::GeometryData` by down-casting `uint32_t` indices to `uint16_t`. Returns `nullptr` and logs an error for meshes exceeding 65 535 vertices.
- **`RegisterMeshData` lambda (inside `main`):** Iterates submeshes, calls `MakeGeometry` for LOD 0, pairs each resulting `GeometryData` with a shared `Material`, creates a `Mesh` + `MeshInstance`, and registers it with the renderer.
- **Demo materials:** `obj_mat` (orange, `demo.png`) and `fbx_mat` (blue, `demo.png`) make the two imports visually distinguishable.
- **Placement:** OBJ at (−10, 0, 0) × scale 3; FBX at (+10, 0, 0) × scale 3 — visible from the default camera position (0, 200, 500).
- **Ownership:** `demo_geos`, `demo_meshes`, `demo_instances` are `vector<unique_ptr<…>>` kept alive for the full render loop lifetime.

---

## Decisions and rationale

| Decision | Rationale |
|---|---|
| `configure_file` for demo.fbx | Avoids committing a binary FBX blob to the repo; reuses the already-fetched ufbx test data. File is regenerated on every CMake configure run. |
| `demo.obj` as a hand-crafted cube | Simple, deterministic, no third-party dependency at data-authoring time. Easily replaced later by a real mesh. |
| `uint16_t` index downcast in `MakeGeometry` | `GeometryData` is currently hardcoded to 16-bit indices (matching the existing cube/sphere usage). A guard + error log covers the edge case without prematurely refactoring the renderer. |
| Per-submesh shared material | Both importers can return multiple submeshes (one per material slot). Sharing a single `Material` per import keeps the demo simple; each submesh still gets its own `GeometryData` and `MeshInstance`. |
| Orange / blue colour coding | Makes it trivially easy to confirm in the viewport which mesh came from which importer. |
| Scale × 3 and ±10-unit offset | The Blender default cube and the hand-crafted OBJ cube are ±1 unit. At scale 1 they would be tiny compared to the 1000-unit world; × 3 makes them clearly visible near the origin without overlapping. |

---

## Output / things to keep in mind

- `data/meshes/demo.fbx` is **generated at CMake configure time** (not committed). A fresh checkout requires running CMake before launching the engine.
- If `demo.obj` or `demo.fbx` are missing at runtime, `Import` returns false, a warning is logged, and the engine continues without crashing.
- The 65 535-vertex limit on `MakeGeometry` will need revisiting if the renderer is upgraded to 32-bit indices (Issue #116 asset pipeline work or later).

---

## Skills used
- `impl-issue`

## Instructions followed
- Google C++ Style Guide (cpplint clean, zero warnings).
- One class per `.h` / `.cpp` (helper struct avoided — used flat vectors + lambda).
- `#include` paths project-relative from `src/`.
- Conventional commit message.
- History file in `history/`.
