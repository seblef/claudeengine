# Fix: Physics Properties Not Saved by Editor

## Problem

The editor could display and edit physics properties for game meshes (shape type,
material friction/restitution/mass, motion type, collision layer/mask) via the
Properties Panel, but those properties were **never written to disk**. On every
map reload all physics configuration was silently lost.

A secondary bug: duplicating a mesh in the editor (`GameMesh::Copy`) did not
propagate the source mesh's `physics_desc_`, so the clone started with no physics.

## Root Cause

`MapSerializer::SerializeVisitor::Visit(game::GameMesh&)` emitted only name,
type, transform, mesh path, and material path — it had no code to write the
`physics` block. `MapLoader::ParsePhysicsBodyDesc` already existed and correctly
parsed the full physics descriptor, so the round-trip was broken only on the
write side.

`GameMesh::Copy` set the clone's name and transform but skipped `physics_desc_`.

## Changes

### `src/editor/MapSerializer.cpp`

Added three helpers inside the anonymous namespace:

- **`ShapeTypeToString`** — maps `PhysicsShapeType` enum to the string tokens
  expected by `MapLoader` (`"box"`, `"sphere"`, `"cylinder"`, `"capsule"`,
  `"convex_hull"`, `"exact"`, `"terrain"`).
- **`MotionTypeToString`** — maps `MotionType` to `"static"` / `"kinematic"` /
  `"dynamic"`.
- **`EmitPhysicsBodyDesc`** — emits the full `physics:` YAML block, matching the
  key names and nesting that `ParsePhysicsBodyDesc` reads:
  - Shape type string + per-shape parameters (half_extents / radius / half_height).
  - `center_offset` (only when non-zero, to keep files clean).
  - `motion_type`.
  - Material fields (friction, restitution, mass, linear_damping, angular_damping,
    gravity_factor) — each omitted when equal to the `PhysicsMaterialDesc` default,
    keeping YAML minimal.
  - `collision_layer` / `collision_mask` — omitted when equal to defaults
    (`kLayerWorld` / `0xFFFF`).

`Visit(game::GameMesh&)` now calls `EmitPhysicsBodyDesc` when `GetPhysicsDesc()`
returns a value.

Added includes for `physics/MotionType.h`, `physics/PhysicsBodyDesc.h`, and
`physics/PhysicsShapeType.h` (already present on the `dev` branch).

### `src/game/GameMesh.cpp`

`Copy()` now forwards `physics_desc_` to the clone via `SetPhysicsDesc`.

## Decisions

- Material properties are written only when they differ from the `PhysicsMaterialDesc`
  defaults. This matches the pattern used for `EnvironmentDesc` in the same file and
  keeps map files readable.
- `center_offset` uses the same conditional-emit approach for the same reason.
- Collision layer/mask use the same approach — `kLayerWorld`/`0xFFFF` defaults are
  implicit in the loader.
- `ConvexHull` and `Exact` shapes have no extra parameters in the YAML (geometry
  is derived from the mesh at runtime), matching the loader expectation.

## Keep in Mind

- The YAML key names in `EmitPhysicsBodyDesc` are the single source of truth for
  the serialization contract — they must stay in sync with `ParsePhysicsBodyDesc`
  in `MapLoader.cpp`.
- `GameMesh::Copy` now calls `SetPhysicsDesc` *after* `SetWorldTransform`. If
  the mesh is already in scene when copied (edge case), `SetPhysicsDesc` will
  create the physics body immediately, which is correct.

## Skills / Instructions Used

- `src/CLAUDE.md`: Google C++ style guide, one class per file, include paths
  project-relative from `src/`.
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; panel
  logic separated from editing logic.
- `CLAUDE.md` (root): conventional commits, cpplint before commit, history file.
