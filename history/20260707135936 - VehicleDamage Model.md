# VehicleDamage Model

Implements GitHub issue #828: `game::VehicleDamage` — progressive per-zone HP tracked from Jolt collision impulse, with a listener/notification mechanism. Backbone of Milestone 3 (scrape, smoke/fire, wreck, mesh swaps all read from it).

## Summary of changes

### `physics/` — collision impulse plumbing (new capability, didn't exist before)

`WRECKONING.md` §6 assumes the game layer can already learn "collision impulse exceeds threshold" per `game/CLAUDE.md`'s note in `GameVehicle`, but no such channel existed: `IPhysicsBodyListener` only reports per-step transforms, and `PhysicsSystem` never registered a `JPH::ContactListener`. This issue had to add that plumbing before `VehicleDamage` could be driven by anything real.

- **`physics/IPhysicsCollisionListener.h`** (new) — `OnCollision(world_point, impulse)`, fired once per new contact (not persisted/resting contact).
- **`PhysicsSystem.cpp`** — new `VehicleContactListener : JPH::ContactListener`, registered via `SetContactListener()` in `Init()`. Uses Jolt's `EstimateCollisionResponse()` in `OnContactAdded` (the officially recommended way to estimate impact strength before the solver has run — see the doc comment on `ContactListener::OnContactAdded` referencing this exact function) and sums `mContactImpulse` across all manifold points. Deliberately ignores `OnContactPersisted` so sliding/resting contact never triggers damage — only genuine new impacts do.
- Routing: `PhysicsSystem` keeps `collision_listeners_` (`unordered_map<BodyID, IPhysicsCollisionListener*>`), populated by `CreateVehicle()`'s new optional `collision_listener` parameter and erased in `DestroyVehicle()`. The map is only mutated outside `Step()`, so it's safe to read from contact callbacks that Jolt may run on worker threads.
- This only wires the vehicle **body** shape (box/convex-hull), not wheels — wheels are raycast-based, not separate Jolt bodies, so a normal driving car never spams contact-start events; only genuine body impacts (walls, other cars) do.

### `physics/VehicleDesc.h` — new `VehicleDamageDesc`

Added `VehicleDamageDesc` (per-zone `zone_max_hp` array of 5, `impulse_to_damage_scale`, `thresholds` array of 4, defaults `{0.4, 0.6, 0.8, 1.0}` matching the four gameplay-effect thresholds in `WRECKONING.md` §6) and a `damage` field on `VehicleDesc`. Fixed-size `std::array` (not `std::vector`) chosen deliberately — zone count and threshold count are fixed by the design doc, and this keeps both the YAML shape and the editor UI simple (no add/remove-row UI needed).

### `game::VehicleDamage` (new, per issue scope)

- **`DamageZone.h`** — `enum class DamageZone { kFront, kRear, kLeft, kRight, kRoof }`, own header so both `VehicleDamage.h` and `IVehicleDamageListener.h` can include it without pulling in the whole model.
- **`IVehicleDamageListener.h`** — `OnDamageThresholdCrossed(zone, threshold, fraction)`. Multiple listeners supported (`AddListener`/`RemoveListener`, non-owning) since several later-milestone systems (steering penalty, engine slowdown, smoke, wreck, mesh swap) all subscribe independently rather than each inspecting collisions.
- **`VehicleDamage.h/.cpp`** — the model itself. Fully Jolt-free and `GameObject`-free (only depends on `core::Vec3f` and `physics::VehicleDesc`), so it's unit-testable without any physics/rendering plumbing.
  - `RegisterImpact(local_point, impulse)`: classifies `local_point` (vehicle body-local space) into a zone via `ClassifyZone()`, applies `impulse * impulse_to_damage_scale` as damage (clamped to `[0, max_hp]`), and fires `OnDamageThresholdCrossed` once per threshold newly crossed (ascending order — a single big impact can cross several at once).
  - `ClassifyZone()`: compares `local_point` normalised by `half_extents` per-axis; largest-magnitude axis wins (+Z=front/-Z=rear, +X=left/-X=right per the existing wheel-position sign convention already established in `hell.vehicle.yaml`; +Y=roof only when dominant *and positive* — an underside/negative-Y-dominant hit falls through to whichever of X/Z is next largest, so every impact still resolves to one of the five zones instead of being silently dropped).

### `game::GameVehicle` — wiring

- Implements `physics::IPhysicsCollisionListener` (in addition to the existing `IPhysicsBodyListener`); `OnCollision()` converts the world-space impact point to body-local space via the existing `core::TransformPoint` helper (already used identically in `PickingUtils.cpp`) and forwards to `damage_->RegisterImpact()`.
- Owns `std::unique_ptr<VehicleDamage> damage_`, constructed in the ctor from `template_->GetVehicleDesc()`; exposed via `GetDamage()` for later-issue subscribers.
- `Activate()` now passes `this` as `CreateVehicle()`'s new `collision_listener` argument.

### YAML round-trip (`.vehicle.yaml`)

Per the acceptance criteria, added a `damage:` section to both parse paths:
- **`VehicleTemplate.cpp`** (runtime load) — new `ParseDamageDesc()` helper.
- **`VehicleEditorWindow.cpp`** (editor load + save + UI) — `LoadFromYaml()`/`SaveToYaml()` mirror the same fields (this file already duplicates `VehicleTemplate.cpp`'s parsing independently for every other section, so kept that existing pattern rather than introducing a shared parser as a one-off refactor). Added a compact `DrawDamageSection()` (5 max-HP drag floats, impulse scale, 4 threshold drag floats) between the existing Physics section and the actions bar — without it, the new data would be file-only and invisible/unreachable from the editor, which would be an obvious gap for the vehicle-authoring workflow this window exists for.

### Tests

`tests/game/VehicleDamageTest.cpp` — 15 tests, compiles `VehicleDamage.cpp` directly and links only `core` (no physics/game linkage needed, matching the `VFXSystemTest` isolation pattern from the prior VFX issue). Covers: zone classification for all 5 zones + the underside-fallback edge case, impulse-proportional damage, no-op on zero/negative impulse, clamping at destroyed (fraction 1.0), no notification below the first threshold, single-threshold crossing, multi-threshold crossing in one impact, full-destruction crossing all four, `RemoveListener`, and that unaffected zones never notify.

## Verification performed

- `cpplint` clean on every new/changed file.
- Full rebuild: `physics`, `game`, `wreckoning_editor` (validates the `VehicleEditorWindow.cpp` changes too), `game_tests`, `mesh_tests`, `vfx_tests` — all succeed.
- `game_tests` (28/28, including the 15 new `VehicleDamageTest`), `mesh_tests` (15/15), `vfx_tests` (3/3) all pass.
- **Not done**: no live in-editor/in-game verification that a real Jolt collision actually reaches `VehicleDamage::RegisterImpact` end-to-end (no display available in this environment). The physics→game wiring (`VehicleContactListener` → `IPhysicsCollisionListener` → `GameVehicle::OnCollision` → `VehicleDamage::RegisterImpact`) is exercised by types/compilation and the model logic itself is unit-tested, but the actual Jolt `EstimateCollisionResponse` impulse magnitudes have not been sanity-checked against a real collision — flag for manual QA (drive into a wall, log `GetDamageFraction()`) before relying on the default `impulse_to_damage_scale = 0.05` for tuning.

## Output to keep in mind for next Milestone 3 issues

- `GameVehicle::GetDamage()` is the subscription point — the steering-penalty, engine-slowdown, smoke, wreck-state and mesh-swap issues should all call `vehicle->GetDamage().AddListener(this)` rather than inspecting collisions themselves.
- `VehicleDamage` only *notifies*; it does not itself reduce steering, cap speed, spawn smoke, or swap meshes. Those thresholds (40% front / 60% "engine" i.e. front / 80% any / 100% any per `WRECKONING.md` §6) are gameplay-effect issues layered on top of the generic `OnDamageThresholdCrossed` callback — there's no zone called "engine", front-zone damage is expected to stand in for it.
- `VehicleContactListener` only fires for the vehicle **chassis** body. If a future issue needs collision events for pedestrians or props, the same `collision_listeners_` map / `IPhysicsCollisionListener` plumbing generalises directly — no new Jolt-side work needed, just pass a listener into whichever `CreateBody*` call is relevant (not currently wired for `CreateBody`/`CreateBodyWithMesh`, only `CreateVehicle`, since nothing else needed it yet).
- Default `impulse_to_damage_scale = 0.05` and thresholds `{0.4, 0.6, 0.8, 1.0}` are unvalidated placeholders — expect retuning once real collisions can be observed.

## Propositions

**CLAUDE.md improvements** (not applied, proposing only):
- `src/physics/CLAUDE.md`'s module table doesn't mention `IPhysicsCollisionListener` or the new contact-listener wiring in `PhysicsSystem.cpp` — worth a row once a second consumer exists (e.g. pedestrian ragdoll trigger), to avoid re-deriving the impulse-estimation approach from scratch.
- `src/game/CLAUDE.md`'s module table doesn't list `VehicleDamage`, `DamageZone`, or `IVehicleDamageListener` — worth adding once the smoke/wreck/mesh-swap issues land and this becomes a hub other systems reference.

**README.md**: no build/install process changes (no new third-party library or build option) — no proposal needed.

**Specification questions**: none blocking — `WRECKONING.md` §6, the existing `IPhysicsBodyListener`/`CreateVehicle` patterns, and Jolt's own `EstimateCollisionResponse` documentation were sufficient to resolve the impulse-estimation approach, the zone-classification geometry, and the threshold-notification shape without asking. One judgment call worth flagging explicitly: the issue's "60% engine damage" gameplay effect has no dedicated "engine" zone in the 5-zone model from §6 (Front/Rear/Left/Right/Roof) — proceeded on the assumption that front-zone damage represents engine damage, since later issues consume this generically anyway.

## Skills used

- `impl-issue` (invoked via `/impl-issue 828`).

## CLAUDE.md instructions especially taken into account

- `src/CLAUDE.md`: one class per `.h`/`.cpp`, project-relative `#include` paths, utility functions in a single file.
- `src/physics/CLAUDE.md`: Jolt types must never appear in a `physics/*.h` included by `game/`/`editor/` — kept `IPhysicsCollisionListener.h` and the `VehicleDesc.h` additions Jolt-free; all `<Jolt/...>` includes stayed in `PhysicsSystem.cpp`.
- `src/game/CLAUDE.md`: `VehicleDamage` is a plain component-like class (not a `GameObject`) since it has no independent scene presence — matches the existing scene-object-vs-component distinction already documented there.
- `src/editor/CLAUDE.md`: panel classes must be pure UI — `DrawDamageSection()` only reads/writes `vehicle_desc_.damage` fields directly (all plain floats, no algorithm to extract) and sets `dirty_`, consistent with the existing `DrawPhysicsSection()` it sits next to.
- Root `CLAUDE.md` git workflow: branched from freshly-pulled `dev`, ran `cpplint`, conventional-commit message, PR to `dev` with closing keyword.
