# OBJ Import via fast_obj

**Issue**: #113 — Mesh Pipeline v1 milestone
**Branch**: feat/mesh-obj-importer

## Changes

| File | Change |
|---|---|
| `CMakeLists.txt` | FetchContent for `fast_obj` (master, shallow) |
| `src/mesh/ObjImporter.h` | `ObjImporter : IMeshImporter` declaration |
| `src/mesh/ObjImporter.cpp` | Implementation + `FAST_OBJ_IMPLEMENTATION` defined here |
| `src/mesh/CMakeLists.txt` | Added `ObjImporter.cpp`; linked `fast_obj` |
| `tests/mesh/CMakeLists.txt` | `mesh_tests` executable; `MESH_TEST_FIXTURES_DIR` compile def |
| `tests/mesh/ObjImporterTest.cpp` | 6 unit tests (failure, LOD count, vertex/index counts, normals, AABB) |
| `tests/mesh/fixtures/quad.obj` | 4-vertex horizontal quad (2 triangles, 1 normal, 4 UVs) |
| `tests/CMakeLists.txt` | Added `mesh` subdirectory |

## Implementation

`ObjImporter::Import` follows this pipeline per group:

1. `fast_obj_read` — parse the OBJ file
2. One `SubmeshData` per non-empty OBJ group; material slot set to first face's material name
3. Fan triangulation for N-gon faces: `(0,1,2), (0,2,3), …`
4. `WeldVertices` — deduplicate expanded vertex list
5. `ComputeNormals` — only if the OBJ file provides no `vn` directives
6. `ComputeTangents` — always
7. Compute AABB from welded vertex positions

## Decisions

- **`FAST_OBJ_IMPLEMENTATION` in ObjImporter.cpp** — single-header implementation pattern; defined in only one TU to avoid ODR violations.
- **Group → one LOD only** — OBJ files carry a single mesh; LOD authoring is explicit in source files per the format spec.
- **Sequential pre-weld indices** — all vertices are pushed with indices `0,1,2,…` before welding. `WeldVertices` then discovers duplicates by hash-key lookup across the index list. This is simpler than deduplicating during expansion.
- **`fast_obj.h` included with angle brackets** — cppcheck's `missingIncludeSystem` suppression covers the missing header during staged-only analysis; the compiler finds it via the `fast_obj` INTERFACE target.
- **`const char[]` for global test strings** — cpplint `runtime/string` rule; C-style string literals are preferred for global/static string constants.

## Test results

```
6 tests from 2 test suites — PASSED
  ObjImporterTest.MissingFileReturnsFalse
  ObjImporterQuadTest.OneLod
  ObjImporterQuadTest.VertexCountAfterWeld   (4 verts after weld)
  ObjImporterQuadTest.IndexCount             (6 indices)
  ObjImporterQuadTest.NormalsPointUp         (0,1,0 preserved from vn)
  ObjImporterQuadTest.AabbCoversQuad         (XZ [0,1]×[0,1])
```

## Next steps

- **Issue #114**: `FbxImporter : IMeshImporter` using `ufbx` — same pipeline, different parser.
- **Issue #115**: Load `demo.obj` and `demo.fbx` at startup in the engine demo.

## Skills and guidelines used

- `impl-issue` skill workflow
- `src/CLAUDE.md`: one class per .h, Google C++ style, `#include` root is `src/`
- `CPPLINT.cfg`: `runtime/string` applies to global string constants — use `const char[]`
