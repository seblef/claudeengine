# FBX import via ufbx

**Date:** 2026-05-12  
**Issue:** #114 — [feat] FBX import via ufbx  
**Branch:** feat/mesh-fbx-importer  
**Milestone:** Mesh Pipeline v1

---

## Summary

Integrated the `ufbx` single-file FBX library and implemented `FbxImporter`, a concrete `IMeshImporter` that loads FBX scenes, triangulates geometry, separates material parts into submeshes, and runs the standard mesh pipeline (WeldVertices → ComputeNormals → ComputeTangents → AABB).

---

## Changes

### `CMakeLists.txt` (root)
- Added `LANGUAGES C` to `project()` — required to compile `ufbx.c` (plain C).
- Added `FetchContent_Declare` / `FetchContent_MakeAvailable` for `ufbx` (GitHub: `ufbx/ufbx`, tag `master`).
- Manually created a `ufbx` STATIC library target from `${ufbx_SOURCE_DIR}/ufbx.c` with `-w` / `/W0` to suppress warnings in third-party code.

### `src/mesh/FbxImporter.h`
New header declaring `FbxImporter : public IMeshImporter`.

### `src/mesh/FbxImporter.cpp`
Full implementation:
- `ufbx_load_file` / `ufbx_free_scene` for RAII-like scene lifecycle.
- Iterates `scene->meshes`; skips empty meshes.
- Separates geometry by `mesh->material_parts` into one `SubmeshData` per part; falls back to a single submesh if no material parts exist.
- `ufbx_triangulate_face` converts quads/ngons into triangles; indices feed `ufbx_get_vertex_vec3` / `ufbx_get_vertex_vec2`.
- `ufbx_real` (double) is cast to `float` via `static_cast`.
- Material slot name resolved from `m->face_material` → `m->materials[idx]->name`.
- Per-submesh pipeline: expand vertices → `WeldVertices` → `ComputeNormals` (if no normals in file) → `ComputeTangents` → `ComputeAabb`.

### `src/mesh/CMakeLists.txt`
Added `FbxImporter.cpp` to the `mesh` library and linked `ufbx`.

### `tests/mesh/FbxImporterTest.cpp`
Six tests using `ufbx`'s bundled `blender_279_default_6100_ascii.fbx` (Blender 2.79 default cube):
1. `MissingFileReturnsFalse` — non-existent path returns false.
2. `OneLod` — single submesh with one LOD.
3. `VertexCountAfterWeld` — 24 vertices (6 quads × 4 unique verts; flat normals prevent cross-face merging).
4. `IndexCount` — 36 indices (12 triangles × 3).
5. `NormalsAreUnitLength` — all normals within 1e-4 of unit length.
6. `AabbContainsOrigin` — min ≤ 0 and max ≥ 0 on all axes.

### `tests/mesh/CMakeLists.txt`
Added `FbxImporterTest.cpp` to `mesh_tests` and added `UFBX_DATA_DIR="${ufbx_SOURCE_DIR}/data"` compile definition so tests can reference ufbx bundled FBX files without copying them.

---

## Decisions and rationale

| Decision | Rationale |
|---|---|
| `ufbx` over Assimp for in-engine FBX | Single `.c` file, MIT licensed, zero dependencies, fast compilation. Assimp reserved for the offline CLI converter (more formats needed there). |
| Manual `ufbx` CMake target | ufbx ships no `CMakeLists.txt`; `FetchContent_MakeAvailable` alone wouldn't produce a linkable target. |
| `LANGUAGES C CXX` in project() | `ufbx.c` is plain C; without enabling the C language CMake cannot compile it. |
| `-w` / `/W0` on ufbx | Third-party C code with strict C++ warning flags would produce noise; suppressing at the target level is idiomatic. |
| Test uses ufbx bundled FBX | Avoids copying a binary blob into the repo; `UFBX_DATA_DIR` points directly into the FetchContent source. Blender 2.79 default cube has well-known geometry counts that can be analytically verified. |
| Vertex count = 24 (not 8) | `ByPolygonVertex/Direct` normals in the file assign the same flat normal to all 4 corners of each face. After welding on (position, normal, UV), corners from adjacent faces that share a position but differ in normal cannot merge → 6 faces × 4 corners = 24. |

---

## Key API notes for ufbx

- `ufbx_load_file(path, opts, error)` → `ufbx_scene*` (null on failure).
- `ufbx_free_scene(scene)` must always be called.
- `mesh->vertex_normal.exists` / `mesh->vertex_uv.exists` indicate attribute presence.
- `mesh->material_parts` splits faces by material; `part.face_indices` provides the per-part face indices.
- `ufbx_triangulate_face(buf, buf_size, mesh, face)` returns `num_tris`; resulting `ix` values work directly with `ufbx_get_vertex_*`.
- `ufbx_real` is `double` — always cast to `float`.

---

## Skills used
- `impl-issue`

## Instructions followed
- Google C++ Style Guide (cpplint clean, zero warnings).
- One class per `.h` / `.cpp`.
- `#include` paths project-relative from `src/`.
- Conventional commit message.
- History file in `history/`.
