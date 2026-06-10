# GeometryData submesh ranges

**Issue:** #422  
**PR:** #432  
**Branch:** feat/renderer-geometry-submesh-ranges  

## Changes

### `src/renderer/GeometryData.h` (modified)

- Added `#include <vector>` and `#include "mesh/SubMeshRange.h"`
- Constructor gains an optional `std::vector<mesh::SubMeshRange> submeshes = {}` parameter — default preserves all existing call sites with no changes
- New accessors:
  - `int GetSubMeshCount() const` — returns 0 when no ranges are present (legacy mode)
  - `const mesh::SubMeshRange& GetSubMesh(int i) const`
- Private member `std::vector<mesh::SubMeshRange> submeshes_` added

### `src/renderer/GeometryData.cpp` (modified)

- Added `#include <utility>`, `#include <vector>`, `#include "mesh/SubMeshRange.h"`
- Constructor initialiser list extended: `submeshes_(std::move(submeshes))`

### `src/renderer/MeshLoader.cpp` (modified)

- `MakeGeometry()` helper now passes `lod.submeshes` as the fifth argument to `GeometryData`, forwarding whatever the importer populated

## Decisions

- **Default empty vector = legacy behaviour:** avoids any source-level breakage. Callers that don't care about submeshes never need to change.
- **Accessors return count 0 for legacy meshes:** renderers can guard with `if (geo->GetSubMeshCount() > 0)` to opt into multi-draw without special-casing null.
- **No renderer draw-call splitting in this PR:** actual use of submesh ranges in draw calls is deferred to a follow-up issue; this PR only threads the data through.

## Output to keep in mind

- `GetSubMeshCount() == 0` means "single flat geometry" — renderers should treat this as a full-range draw, not as "no geometry".
- When implementing multi-draw, iterate `[0, GetSubMeshCount())` and use `GetSubMesh(i).index_offset` / `index_count` for each draw call's offset/count pair.

## Skills / CLAUDE.md references used

- `src/CLAUDE.md` — one-class-per-header, include-root convention, cppcheck-suppress pattern
- `impl-issue` skill
