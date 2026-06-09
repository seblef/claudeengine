# FbxImporter: submesh groups and unit conversion

## Summary

Implements issue #424. `FbxImporter` now preserves per-material geometry groups
as `SubMeshRange` entries and exposes the scene unit scale via `MeshData`.

## Changes

### `src/mesh/MeshData.h`
Added `float unit_meters = 1.0f` field.  Defaults to metres; importers that
know better (FBX) overwrite it.  OBJ/emesh paths are unaffected (they leave the
default).

### `src/mesh/FbxImporter.h`
Updated class-level doc comment to reflect the new submesh and unit-scale
behaviour.  No API surface change.

### `src/mesh/FbxImporter.cpp`
Two main additions:

**Unit conversion**: `mesh->unit_meters` is set from
`scene->settings.unit_meters` immediately after the scene is loaded.  No scale
is applied to vertex positions — the import UI is expected to handle that.

**Submesh groups**: The face loop now iterates `m->material_parts` (always
populated by ufbx, even for single-material meshes) instead of `m->faces`
directly.  For each part:
- `face_indices` drives face iteration and triangulation.
- `index_offset` / `index_count` are computed from the running `lod.indices`
  size before/after each part.
- The material name comes from `m->materials.data[part.index]->name`; empty
  string when no material is assigned.
- A `SubMeshRange` is appended only when `index_count > 0` (skips degenerate
  parts that triangulate to nothing).

A fallback branch handles the edge case where `material_parts.count == 0`
(theoretically impossible per ufbx docs, but defensive), emitting one unnamed
submesh covering all faces.

Vertices still share a single interleaved buffer; only index ranges differ per
submesh — consistent with the `SubMeshRange` design.

## Decisions

- **`unit_meters` on `MeshData` rather than a new return struct**: `MeshData`
  is already the output parameter of `Import()`; adding a field keeps the
  `IMeshImporter` interface unchanged and lets OBJ/emesh callers ignore it
  cheaply.
- **No silent unit conversion**: the issue explicitly requires the scale to be
  applied by the import window, not silently by the importer.
- **Fallback for no `material_parts`**: ufbx guarantees the list is always
  populated, but the fallback avoids a silent no-geometry result if the
  guarantee ever breaks.

## Output to keep in mind

- `MeshData::unit_meters` is now the canonical place to carry the import-time
  unit scale.  Future importers (e.g. OBJ with `Mm` scale) should write it too.
- The import UI (`MeshImportWindow`) should read `MeshData::unit_meters` and
  pre-fill the scale field.  It currently does not.
- `FbxImporterCubeTest` confirms single-material FBX still produces exactly one
  `SubMeshRange`.

## Skills / CLAUDE.md instructions followed

- Checked out `dev` and pulled before branching.
- Branch prefixed `feat/`.
- `cpplint` passed on all three modified files.
- Commit message follows Conventional Commits.
- PR targets `dev` with `Closes #424`.
