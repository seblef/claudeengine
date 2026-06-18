# physics: expose DrawDebug API on PhysicsSystem (#624)

## Summary

Added `PhysicsDebugDrawSettings` and `PhysicsSystem::DrawDebug()` so the editor
or game can trigger Jolt's built-in debug draw passes each frame without exposing
any Jolt types in the public header.

## Changes

### `src/physics/PhysicsSystem.h`
- Added `PhysicsDebugDrawSettings` struct (Jolt-free, lives inside `namespace physics`):
  - `selectedBodies` — optional filter; null = draw all non-terrain bodies.
  - `drawConstraints` — whether to call `DrawConstraints()`.
  - `drawContactPoints` — whether to enable contact-point rendering during the next `Step()`.
  - `drawBroadPhase` — whether to draw the broadphase world AABB.
- Added `DrawDebug(const PhysicsDebugDrawSettings&)` to the public `PhysicsSystem` API.

### `src/physics/PhysicsSystem.cpp`
- Added `BodyDebugFilter` (anonymous namespace, `JPH::BodyDrawFilter` subclass):
  - Always returns `false` for any body on `kLayerWorld` (terrain/static-world layer).
  - When `has_selection_` is true, additionally excludes bodies whose packed BodyID
    is not in the pre-computed `selected_ids_` set.
- Implemented `PhysicsSystem::DrawDebug()`:
  - Pre-computes a `std::unordered_set<uint32_t>` of selected body IDs from the
    caller-supplied `PhysicsBody*` pointers (uses `body_id_` via friend access).
  - Calls `DrawBodies` with wireframe mode and the filter.
  - Conditionally calls `DrawConstraints`.
  - Sets/clears `ContactConstraintManager::sDrawContactPoint` — contact drawing
    happens inside `Step()` when this flag is true, so the flag is updated here
    and takes effect on the next simulation tick.
  - Conditionally draws the broadphase AABB via `DrawWireBox`.
- Added includes: `<unordered_set>`, `<Jolt/Physics/Body/BodyFilter.h>`,
  `<Jolt/Physics/Body/BodyManager.h>`, `<Jolt/Physics/Collision/BroadPhase/BroadPhaseQuery.h>`.

## Key decisions

### Terrain = kLayerWorld
The issue specifies "exclude terrain layer bodies". Terrain bodies are always
created with `kLayerWorld`; this is also the layer used for any static world
geometry. Excluding `kLayerWorld` matches the documented purpose and avoids the
need to add a terrain flag to `PhysicsBody`.

### Contact-point flag vs. explicit draw call
Jolt draws contact manifolds from inside its solver (`ContactConstraintManager`),
controlled by a static flag. There is no separate `DrawContactPoints()` API.
`DrawDebug()` sets `ContactConstraintManager::sDrawContactPoint` so the caller
drives the flag each frame; if `DrawDebug` is not called on a given frame, the
flag retains the last value (safe for an editor that calls it every frame).

### Wireframe shape rendering
`draw_settings.mDrawShapeWireframe = true` is hard-coded because this is a debug
overlay API; filled shapes would occlude the scene. A future issue can expose
per-call control if needed.

## Output to keep in mind

- `DrawDebug` is entirely in `PhysicsSystem.cpp`; no Jolt types in the header.
- The `BodyDebugFilter` is local to the anonymous namespace in `.cpp`; safe to
  extend if additional filter criteria are needed.
- `drawContactPoints` has a one-frame lag by design (Jolt limitation).

## Skills / instructions referenced
- `src/CLAUDE.md`: one class per .h/.cpp, Google C++ style guide, include root is `src/`.
- `src/physics/CLAUDE.md`: Jolt types must not appear in public headers; all
  `#include <Jolt/…>` kept in `.cpp`.
- Global `CLAUDE.md`: conventional commits, history file, cpplint before commit.
