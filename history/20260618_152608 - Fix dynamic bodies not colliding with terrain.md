# Fix: Dynamic Physics Bodies Not Colliding with Terrain

## Problem

Dynamic physics bodies (e.g., barrels) placed above the terrain fell through it endlessly. Their logged position decreased without bound, confirming they never made contact with the terrain's collision shape.

## Root Cause

Three components combined to produce the bug:

1. **Terrain body is on `kLayerWorld` (0)** — `PhysicsSystem::CreateTerrainBody` hard-codes `kLayerWorld`.

2. **`ObjectLayerPairFilterTable` disables same-layer world collisions** — The init loop in `LayerFilters` skips enabling `kLayerWorld` ↔ `kLayerWorld` to prevent expensive static-static checks:
   ```cpp
   if (a == kLayerWorld && b == kLayerWorld) continue;
   ```

3. **`MapLoader` defaults all bodies to `kLayerWorld`** — When a YAML physics block omits `collision_layer`, `MapLoader::ParsePhysicsBodyDesc` used `physics::kLayerWorld` as the default for every body regardless of motion type. Dynamic bodies therefore ended up on the same layer as terrain, and the pair filter prevented all contact.

The `collision_mask` field in `PhysicsBodyDesc` was a red herring: it is stored in the struct but never forwarded to Jolt's body creation settings — all filtering is done by the static `ObjectLayerPairFilterTable`.

## Changes

### `src/physics/CollisionLayer.h`
- Added `kLayerDynamic = 4` for generic physics-driven world objects (crates, barrels, debris, etc.).
- Incremented `kLayerCount` from 4 → 5.

### `src/physics/PhysicsSystem.cpp`
- Added `bp_iface->MapObjectToBroadPhaseLayer(kLayerDynamic, kBPLayerDynamic)` so the new layer participates in the dynamic broad-phase bucket.
- Added `kLayerDynamic` bit to `kDynamicBits` inside `RaycastBroadPhaseFilter::ShouldCollide` so raycasts can reach dynamic-layer bodies.
- No change to the pair-filter loop: it iterates up to `kLayerCount`, so `kLayerDynamic` ↔ `kLayerWorld` collisions are automatically enabled.

### `src/game/MapLoader.cpp`
- In `ParsePhysicsBodyDesc`, the default `collision_layer` is now `kLayerDynamic` for `Dynamic`/`Kinematic` bodies and `kLayerWorld` for `Static`. Existing maps that already specify an explicit `collision_layer` are unaffected.

## Decisions and Rationale

- **New layer vs. reusing an existing one**: reusing `kLayerPlayer` or `kLayerEnemy` for inanimate props would be semantically wrong and would complicate future gameplay filtering (e.g., players ignoring enemy bodies). A dedicated `kLayerDynamic` is the correct abstraction.
- **Default in MapLoader vs. enforcing in PhysicsSystem**: fixing the default at load time is the least-invasive approach. PhysicsSystem is a low-level wrapper that shouldn't impose policy; MapLoader is the boundary where game semantics (motion type) are available.
- **No map file changes needed**: the demo barrel has no `collision_layer` entry, so it automatically picks up the new default (`kLayerDynamic`) after the loader fix.

## Things to Keep in Mind

- The `collision_mask` field in `PhysicsBodyDesc` / YAML is currently unused by Jolt. If per-body mask filtering is needed in the future, a custom `ObjectLayerPairFilter` subclass or Jolt's group-filter API must be wired up.
- The editor's `PropertiesPanel` already clamps the layer input to `[0, kLayerCount - 1]`, so the new layer (4) is automatically available in the UI.
- `MapSerializer` only omits `collision_layer` when it equals `kLayerWorld`. Dynamic bodies saved after this fix will have `collision_layer: 4` written explicitly, which is correct and forwards-compatible.

## Skills / CLAUDE.md Instructions Used

- Followed the **Git workflow** from `CLAUDE.md` (branch already created: `fix/player-start-position-650`).
- History file written to `history/` as required.
- No new files created beyond the history entry; only existing source files modified.
