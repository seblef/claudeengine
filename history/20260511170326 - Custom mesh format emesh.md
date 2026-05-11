# Custom Mesh Format (.emesh)

**Issue**: #111 — Mesh Pipeline v1 milestone
**Branch**: feat/mesh-emesh-format

## Changes

New module `src/mesh/` containing:

| File | Role |
|---|---|
| `LodData.h` | POD struct: vertex buffer, index buffer, AABB for one LOD |
| `SubmeshData.h` | POD struct: material slot name + vector of LodData |
| `MeshData.h` | POD struct: vector of SubmeshData |
| `EmeshWriter.h/.cpp` | Serialises MeshData → binary .emesh file |
| `EmeshReader.h/.cpp` | Deserialises .emesh file → MeshData |
| `CMakeLists.txt` | Static library `mesh`, links against `core` |

`src/CMakeLists.txt` updated to add `mesh` subdirectory.

## Binary Format Spec

```
Header (16 bytes, little-endian):
  magic[4]       : 'E','M','S','H'
  version        : uint32 = 1
  submesh_count  : uint32
  lod_count      : uint32   (same for all submeshes)

Per submesh (variable):
  name_len       : uint32
  name           : char[name_len]   (no null terminator)
  Per LOD:
    vertex_count : uint32
    index_count  : uint32
    aabb_min     : float[3]
    aabb_max     : float[3]
    vertices     : VertexOnDisk[vertex_count]   (56 bytes each)
    indices      : uint32[index_count]

VertexOnDisk (56 bytes):
  px py pz   (position,  3×float)
  nx ny nz   (normal,    3×float)
  bx by bz   (binormal,  3×float)
  tx ty tz   (tangent,   3×float)
  u  v       (UV,        2×float)
```

**Endianness**: little-endian only (x86/x64 Windows and Linux).
**Vec3f padding**: `core::Vec3f` is 16-byte aligned with a hidden `w_` field. The on-disk `VertexOnDisk` struct stores only the 3 meaningful floats per field (56 bytes total), stripping the padding.

## Decisions

- **LOD count in header** (global) — follows the issue spec. All submeshes carry the same number of LODs; the writer validates this on entry.
- **Plain `float[3]` on disk** instead of `Vec3f` — avoids writing the SIMD alignment padding and keeps the format platform-independent.
- **`std::fstream` binary I/O** — idiomatic C++; a thin `ReadField<T>`/`WriteField<T>` template eliminates cast boilerplate.
- **Separate reader and writer classes** — each .cpp/.h pair has a single responsibility, matching the project convention.
- **`loguru.hpp` included directly** — `core/Logger.h` is a lifecycle facade only; `LOG_F` macros require the loguru header in each TU.

## Next steps

- **Issue #112**: `IMeshImporter` interface + `MeshData` shared utilities (normals, tangent recomputation, vertex welding).
- **Issues #113/#114**: `ObjImporter` and `FbxImporter` populate `MeshData` and can then be round-tripped through `EmeshWriter`/`EmeshReader`.

## Skills and guidelines used

- `impl-issue` skill workflow (checkout dev → feat branch → implement → cpplint → commit → PR)
- `src/CLAUDE.md`: one class per .h, one class per .cpp, Google C++ style, include root is `src/`
- `CPPLINT.cfg`: linelength=120, suppressed legal/copyright and pragma-once guard checks
