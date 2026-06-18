# PhysicsSystem Raycast API

**Issue:** #569  
**PR:** #616  
**Branch:** `feat/physics-raycast-api-569`

## Changes

### `src/physics/PhysicsSystem.h`
- Added `<optional>`, `core/Vec3f.h`, and `physics/RaycastResult.h` includes.
- Added `Raycast(origin, direction, max_dist, layer_mask = 0xFFFF) const` under a new `// ---- Query` section.

### `src/physics/PhysicsSystem.cpp`
- Added Jolt headers: `CastResult.h`, `NarrowPhaseQuery.h`, `RayCast.h`, `BodyLock.h`, `RaycastResult.h`.
- Two new helper classes in the anonymous namespace:
  - `RaycastBroadPhaseFilter` — passes a broad-phase bucket when at least one of its object layers is set in `layer_mask`. Maps `kBPLayerStatic → kLayerWorld (bit 0)` and `kBPLayerDynamic → kLayerPlayer|kLayerEnemy|kLayerProjectile (bits 1-3)`.
  - `RaycastObjectLayerFilter` — passes a body when `(layer_mask >> layer) & 1`.
- `Raycast()` implementation:
  1. Builds `JPH::RRayCast{origin, direction * max_dist}`.
  2. Calls `NarrowPhaseQuery::CastRay` with both filters; returns `nullopt` on miss.
  3. Computes `distance = mFraction * max_dist`, `position = origin + direction * distance`.
  4. Locks body with `BodyLockRead` → calls `Body::GetWorldSpaceSurfaceNormal`.
  5. Resolves `BodyID` → `PhysicsBody*` via `std::find_if` over `bodies_`.

## Decisions

**Normal via `Body::GetWorldSpaceSurfaceNormal` not `BodyInterface`:** The issue spec referenced a non-existent `BodyInterface` overload. The actual Jolt API lives on `Body` itself, requiring a read-lock. The `BodyLockRead` is RAII and released before the function returns.

**Filter helpers as anonymous-namespace classes:** keeps all Jolt types out of `PhysicsSystem.h`, consistent with the existing `LayerFilters` struct.

**Linear scan for BodyID → PhysicsBody*:** acceptable for the expected body count (< a few thousand). A `std::unordered_map<uint32_t, PhysicsBody*>` can be added later if profiling shows it as a bottleneck.

**`std::find_if` instead of raw loop:** required by the pre-commit cppcheck hook.

## Skills / CLAUDE.md rules applied

- `src/physics/CLAUDE.md`: Jolt types kept out of public headers; one class per file.
- `src/CLAUDE.md`: Google C++ style, include root is `src/`.
- Root `CLAUDE.md`: conventional commits, history file, PR with closing command.

## Output to keep in mind

- The `BroadPhaseLayerFilter` and `ObjectLayerFilter` interfaces are defined in `Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h` and `Jolt/Physics/Collision/ObjectLayer.h` respectively; both are transitively included via `NarrowPhaseQuery.h`.
- The broad-phase bucket mapping (static ↔ kLayerWorld, dynamic ↔ everything else) must be kept in sync with `LayerFilters::bp_iface` in `PhysicsSystem.cpp` if new layers are added.
- If `bodies_` grows large, replace the linear scan with an `std::unordered_map<uint32_t, PhysicsBody*>` keyed on `body_id_`.
