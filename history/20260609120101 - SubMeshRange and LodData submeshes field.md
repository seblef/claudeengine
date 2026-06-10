# SubMeshRange and LodData submeshes field

**Issue:** #421  
**PR:** #431  
**Branch:** feat/mesh-submesh-range  

## Changes

### `src/mesh/SubMeshRange.h` (new file)

Declares `mesh::SubMeshRange`:

```cpp
struct SubMeshRange {
  uint32_t index_offset;
  uint32_t index_count;
  std::string material_name;
};
```

All three members carry `// cppcheck-suppress unusedStructMember` to match the existing convention in `LodData.h` and `MeshData.h` (members are read only in .cpp consumers, not in headers, which triggers the cppcheck false-positive).

### `src/mesh/LodData.h` (modified)

- Added `#include "mesh/SubMeshRange.h"`
- Added `std::vector<SubMeshRange> submeshes;` field with cppcheck suppression
- Updated struct doc-comment to explain the empty-vector legacy fallback

`MeshData.h` is unchanged — it wraps `LodData` by value and gains the field automatically.

## Decisions

- **Separate header for SubMeshRange:** follows the one-class-per-header rule in `src/CLAUDE.md`.
- **Empty vector = legacy behaviour:** avoids any breaking change; all existing importers (FBX, OBJ, emesh) already produce flat `LodData` without populating `submeshes`.
- **No changes to importers in this PR:** populating `submeshes` is deferred to per-importer follow-up issues (the milestone "Enhanced Mesh Import & Submesh Support").

## Output to keep in mind

- Downstream renderers that consume `LodData` will need a guard: `if (!lod.submeshes.empty())` to opt into multi-draw behaviour.
- `MeshData` comment ("no submeshes") is now slightly stale — consider updating it when the first importer actually fills `submeshes`.

## Skills / CLAUDE.md references used

- `src/CLAUDE.md` — one-class-per-header rule, include-root convention, cppcheck-suppress pattern
- `impl-issue` skill
