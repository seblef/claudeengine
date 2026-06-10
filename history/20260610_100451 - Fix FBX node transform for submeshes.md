# Fix: FBX Node Transform for Submeshes

**Branch:** `fix/fbx-node-transform-submeshes`  
**Date:** 2026-06-10

## Problem

FBX files containing multiple mesh objects placed at different positions/rotations were imported with all submesh geometry collapsed to the origin (or at arbitrary wrong positions). Parts of multi-object FBX models appeared "in creepy positions."

## Root Cause

`FbxImporter::Import` iterated `scene->meshes` directly. In ufbx, `scene->meshes` is the raw mesh library — each entry stores geometry in object (local) space, with no awareness of which scene node references it or what transform that node carries. The per-node translation, rotation, and scale (encoded in `ufbx_node::geometry_to_world`) were completely ignored.

## Fix

Switch iteration from `scene->meshes` to `scene->nodes`. For each node that owns a mesh:

1. Read `node->geometry_to_world` — a `ufbx_matrix` that composes the geometry offset with the full world transform.
2. Derive the normal matrix via `ufbx_matrix_for_normals(&world_mat)` — this is the inverse transpose, required for correct normal transformation under non-uniform scaling.
3. Apply `ufbx_transform_position(&world_mat, raw_pos)` to every vertex position.
4. Apply `ufbx_transform_direction(&normal_mat, raw_normal)` to every vertex normal.

The rest of the pipeline (material parts, submesh ranges, welding, tangent computation) is unchanged.

## Files Changed

- `src/mesh/FbxImporter.cpp` — core fix; node loop replaces mesh loop, transforms applied per vertex.
- `src/mesh/FbxImporter.h` — doc comment updated to reflect node-based iteration.

## Decisions & Rationale

- **Node iteration handles instancing correctly.** If the same mesh is referenced by two nodes, it appears twice in the output — which is the correct behaviour for instanced objects in a baked export.
- **Normal matrix is required.** Using `world_mat` directly for normals would corrupt them whenever the node has non-uniform scale. `ufbx_matrix_for_normals` computes the proper inverse transpose.
- **No unit-scale conversion.** The existing design intentionally leaves `unit_meters` to the caller; this fix does not change that contract.

## Testing

All 15 mesh tests passed (`build/mesh_tests`). The test suite covers vertex count after weld, index count, unit-length normals, and AABB correctness on a reference Blender FBX cube.

## Things to Keep in Mind

- Any FBX file whose mesh nodes all sit at the origin (identity transform) will produce identical output before and after this fix — no regression risk there.
- Files authored with multiple mesh objects at non-identity transforms will now import correctly.
- If a future test adds a multi-object FBX with known world positions, the AABB check is the simplest way to catch regressions.

## Skills / CLAUDE.md Instructions Used

- `src/CLAUDE.md`: one class per `.cpp`/`.h`, Google C++ style, include root is `src/`.
- Root `CLAUDE.md`: fix branch from dev, cpplint before commit, conventional commit message, history file required.
