# GameMesh Physics Integration (Issue #565)

## Summary

Wired `GameMesh` into the physics system. `GameMesh` now inherits
`physics::IPhysicsBodyListener` and carries an optional `PhysicsBodyDesc`.
When in scene with a physics desc, a `PhysicsBody` is created and kept in sync
with the game-side world transform.

## Part 1 — MeshTemplate CPU data retention

**Already implemented.** `MeshTemplate` already retains `cpu_positions_` and
`cpu_indices_` (added for ray-triangle picking), exposed via `GetCPUPositions()`
and `GetCPUIndices()`. The issue's proposed `mesh::MeshData` pointer approach
was superseded by the existing, cleaner per-array API. No changes needed.

## Part 2 — GameMesh physics integration

### New members (`GameMesh.h`)

| Member | Type | Purpose |
|---|---|---|
| `physics_desc_` | `std::optional<physics::PhysicsBodyDesc>` | Optional body description |
| `physics_body_` | `physics::PhysicsBody*` | Non-owning handle; owned by PhysicsSystem |
| `in_scene_` | `bool` | Tracks scene membership to handle SetPhysicsDesc() while in scene |

### New methods

**`SetPhysicsDesc(const physics::PhysicsBodyDesc&)`** — Stores the desc. If
already in scene, destroys the existing body and recreates it immediately.

**`OnBodyTransformUpdated(const core::Mat4f&)`** — IPhysicsBodyListener
override. Calls `SetWorldTransform()`, propagating simulation-driven position
back to the renderer instance. Only reached for Dynamic bodies.

**`CreatePhysicsBody()` / `DestroyPhysicsBody()`** — Private helpers that
encapsulate PhysicsSystem calls and the nullptr guard.

### Updated methods

**`OnAddedToScene()`** — After registering with Renderer, creates a physics
body if `physics_desc_` has a value.

**`OnRemovedFromScene()`** — Destroys the physics body first, then unregisters
from the Renderer.

**`OnWorldTransformUpdated()`** — Forwards transform to the MeshInstance and,
for Static or Kinematic bodies, pushes the transform into Jolt via
`PhysicsBody::SetWorldTransform()`. Dynamic bodies are excluded — they drive
the game transform, not the other way round.

**`~GameMesh()`** — Calls `DestroyPhysicsBody()` before `template_->Release()`
to ensure no dangling listener pointer remains in PhysicsSystem.

## Decisions

### Reinterpret-cast for vertex data

`MeshTemplate::GetCPUPositions()` returns `std::vector<core::Vec3f>`. Vec3f
is a standard-layout class with three consecutive `float` members (x, y, z)
and no padding. `reinterpret_cast<const float*>(positions.data())` is safe and
avoids a copy of the entire vertex buffer at body-creation time.

### `in_scene_` flag on GameMesh

`GameObject` has no scene-membership tracking. Rather than adding it to the
base class (touching every subclass), a private `in_scene_` bool is kept in
`GameMesh` alone. This is the only game object that needs dynamic late-binding
of a secondary system (physics).

### Terrain shape type guard

`PhysicsSystem::CreateTerrainBody()` takes `TerrainData*`, not a generic desc.
A Terrain `PhysicsShapeType` in a GameMesh context is a mis-use; a warning is
logged and no body is created. `GameTerrain` handles terrain bodies itself.

## Files changed

- `src/game/GameMesh.h` — Added `IPhysicsBodyListener` inheritance, new members,
  `SetPhysicsDesc`, `OnBodyTransformUpdated`, private helpers.
- `src/game/GameMesh.cpp` — Full reimplementation of scene lifecycle methods
  plus new physics integration methods.

## Skills / instructions used

- `impl-issue` skill from `.claude/`
- `src/CLAUDE.md`: one class per file, Google C++ style, Google include path
- `src/game/CLAUDE.md`: dependency graph, GameMesh/MeshTemplate role
- `src/physics/CLAUDE.md`: Jolt-free public API, `game → physics → core` graph

## Notes for next features

- ConvexHull/Exact CPU data is built from `GetCPUPositions()` /
  `GetCPUIndices()` — if the MeshTemplate is procedural (no CPU data), the body
  is silently skipped with a warning.
- Dynamic body updates are one-way: Jolt → `OnBodyTransformUpdated` →
  `SetWorldTransform` → `OnWorldTransformUpdated` → MeshInstance. Make sure
  editor drag logic gates on `!Dynamic` to avoid fighting the simulation.
