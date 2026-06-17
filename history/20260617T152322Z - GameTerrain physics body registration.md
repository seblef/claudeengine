# GameTerrain Physics Body Registration

**Issue**: #567 — feat(game,physics): register terrain heightmap as a static physics body  
**Branch**: feat/game-terrain-physics-567  
**Date**: 2026-06-17

## Summary

`GameTerrain` now registers its heightmap with `PhysicsSystem` as a static
`HeightFieldShape` (via `CreateTerrainBody`) when added to the scene, and
destroys the body when removed.

## Changes

### `game/GameTerrain.h`
- Added forward declaration `namespace physics { class PhysicsBody; }`.
- Added private member `physics::PhysicsBody* terrain_body_ = nullptr;` (non-owning;
  lifetime owned by `PhysicsSystem`).
- Changed `OnWorldTransformUpdated()` from an inline no-op to a declared override
  (implementation in `.cpp`).

### `game/GameTerrain.cpp`
- Added `#include "physics/PhysicsSystem.h"`.
- `OnAddedToScene()`: after renderer init, calls
  `PhysicsSystem::Instance().CreateTerrainBody(data_.get(), GetWorldTransform())`
  guarded by `PhysicsSystem::IsInstanced()` so the terrain still works in
  editor contexts where no physics world is running.
- `OnRemovedFromScene()`: destroys `terrain_body_` before renderer teardown.
- `OnWorldTransformUpdated()`: propagates transform to `terrain_body_` via
  `SetWorldTransform(GetWorldTransform())` if the body exists.

## Decisions

### `IsInstanced()` guard in `OnAddedToScene()`
The editor app may not create a `PhysicsSystem`. The guard lets `GameTerrain`
work in both the game app (physics present) and the editor app (physics absent)
without requiring callers to handle two code paths.

### `world_transform_` is private — use `GetWorldTransform()`
`GameObject::world_transform_` is private, so `OnWorldTransformUpdated()` calls
`GetWorldTransform()` instead of accessing the member directly.

### `CreateTerrainBody` was pre-implemented
`PhysicsSystem::CreateTerrainBody` was already present in `PhysicsSystem.cpp`
from a prior issue. This issue only wired it into `GameTerrain`. The TerrainData
API uses `GetTexelWidth/Height`, `GetMetersPerTexel`, `GetMinHeight/MaxHeight`,
and `GetRawData()` — not `GetWidth/Height/GetHeightSamples` as mentioned in
the issue spec; the implementation is correct.

## Skills used
- `impl-issue`

## Notes for next features
- If the editor ever gains a physics preview mode, the `IsInstanced()` guard
  can be removed once a `PhysicsSystem` is created in the editor app.
- `PhysicsBody::SetWorldTransform` on a static body calls Jolt
  `SetPositionAndRotation` without activation — correct for terrain which never
  moves at runtime.
