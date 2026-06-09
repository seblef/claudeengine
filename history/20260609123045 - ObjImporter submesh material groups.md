# ObjImporter: submesh material groups

## Summary

Implements issue #425. `ObjImporter` now preserves per-material geometry groups
as `SubMeshRange` entries instead of merging all faces into a flat `LodData`.

## Changes

### `src/mesh/ObjImporter.h`
Updated class-level doc comment to reflect the new submesh behaviour. No API
surface change.

### `src/mesh/ObjImporter.cpp`
Replaced the group-based face loop with a single global face loop that reads
`obj->face_materials[fi]` for each face. Contiguous runs of the same material
index produce one `SubMeshRange`. Key points:

- Removed `GlobalVertexOffset` and `AppendGroupGeometry` helpers; the global
  index into `obj->indices` is now tracked with a simple running counter
  (`global_index`), which is simpler and avoids the O(groups × faces) scan that
  `GlobalVertexOffset` was doing.
- `MaterialName()` helper resolves `obj->materials[mat_idx].name`, returning an
  empty string when out of range or null (no `.mtl` file).
- A SubMeshRange is only appended when `index_count > 0` (skips degenerate
  material runs that produce no triangles).
- Log message now includes the submesh count.

### `tests/mesh/ObjImporterTest.cpp`
Added:
- `ObjImporterQuadTest.SingleSubmesh` — verifies that no-material OBJ still
  produces exactly one submesh covering all indices.
- `ObjImporterTwoMatsTest` fixture (4 tests) — verifies two-material OBJ produces
  two SubMeshRanges with correct names, offsets, and counts.

### `tests/mesh/fixtures/two_mats.obj` + `two_mats.mtl`
New test fixture: two quads sharing an edge, one assigned `red`, one `blue`.

## Decisions

- **Contiguous-run approach instead of unique-per-material grouping**: OBJ
  exporters always emit `usemtl` before faces for that material, so
  `face_materials` is naturally organized in contiguous runs. Grouping
  non-contiguous faces with the same material into one range would require
  re-sorting faces, which changes vertex layout and complicates welding.  This
  matches how ufbx's `material_parts` works for FBX.
- **Global face loop instead of group loop**: The group loop was needed when
  material data was ignored and groups were the only organization unit. Now that
  we key on material, the global face array is the right level of abstraction.

## Output to keep in mind

- `ObjImporter` and `FbxImporter` now both use the contiguous-run convention
  for `SubMeshRange`: one entry per run, not one per unique material.
- The import UI should read `LodData::submeshes` to display / select materials
  per submesh.

## Skills / CLAUDE.md instructions followed

- Checked out `dev` and pulled before branching.
- Branch prefixed `feat/`.
- `cpplint` passed on all three modified source files.
- Commit message follows Conventional Commits.
- PR targets `dev` with `Closes #425`.
