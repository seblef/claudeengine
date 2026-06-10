# Merge submeshes with the same material during import

## Problem

Both `ObjImporter` and `FbxImporter` build `SubMeshRange` entries per **contiguous
material run**, not per unique material.  If the same material appears on
non-adjacent faces (common in multi-group OBJ exports and multi-node FBX files)
the importers produce multiple `SubMeshRange` entries sharing the same
`material_name`.  The renderer would issue redundant draw calls and material
binds for the duplicates, and downstream tooling that iterates submeshes would
see unexpected counts.

## Solution

Added `MergeSubmeshesByMaterial(LodData*)` to `MeshUtils`.

**Algorithm:**

1. Early-out if `submeshes.size() <= 1` (nothing to merge).
2. Walk `submeshes` once to record first-seen material order and build
   `mat_to_slot` map (`unordered_map<string, size_t>`).
3. Early-out if every material is already unique (`order.size() ==
   submeshes.size()`).
4. Walk `submeshes` again, appending each range's indices into a per-slot
   `bucket` vector.
5. Rebuild `lod->indices` and `lod->submeshes` from the buckets in
   first-seen order.

Only the index buffer is reorganised; the vertex buffer is untouched.
Calling after `WeldVertices` is intentional — welding re-maps index values
but preserves range boundaries, so merge still sees the correct vertex refs.

## Files changed

| File | Change |
|---|---|
| `src/mesh/MeshUtils.h` | Added `MergeSubmeshesByMaterial` declaration |
| `src/mesh/MeshUtils.cpp` | Implemented `MergeSubmeshesByMaterial`; added `<string>` and `SubMeshRange.h` includes |
| `src/mesh/ObjImporter.cpp` | Call `MergeSubmeshesByMaterial` after `WeldVertices` |
| `src/mesh/FbxImporter.cpp` | Call `MergeSubmeshesByMaterial` after `WeldVertices` |

## Decisions

* **Single utility function, shared by both importers** — avoids duplicating
  merge logic and ensures the fix applies to any future importer that follows
  the same pipeline convention.
* **Preserve first-seen material order** — deterministic output regardless of
  the order faces were encountered; easier to reason about in tests.
* **Early-out on already-unique materials** — zero overhead for the common
  single-material case and for files where each material already forms one
  contiguous block.

## Notes for future contributions

* Any new importer implementing `IMeshImporter` should call the standard
  pipeline: `WeldVertices → MergeSubmeshesByMaterial → ComputeNormals (if needed)
  → ComputeTangents → ComputeAabb`.
* `MergeSubmeshesByMaterial` is safe to call even when `submeshes` is empty
  (single-material legacy path).

## Skills / CLAUDE.md instructions used

* `src/CLAUDE.md` — one class per `.cpp`/`.h`, Google C++ style, include root `src/`
* `CLAUDE.md` (root) — conventional commits, `cpplint` before commit, history file
