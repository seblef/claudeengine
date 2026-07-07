# Crash Sound Effects on Impact

Implements GitHub issue #829: play crash sound effects on vehicle impact,
scaled by collision impulse, using the existing `SoundEffectComponent`
one-shot mechanism and the collision-impulse plumbing added for
`VehicleDamage` (`IPhysicsCollisionListener` / `GameVehicle::OnCollision`).

## Summary of changes

- `physics::CrashSoundDesc` (new, in `physics/VehicleDesc.h`): tunable
  thresholds (`min_impulse`, `medium_impulse`, `heavy_impulse`,
  `max_impulse`), a `debounce_time`, and three sound asset stems
  (`light_sound` / `medium_sound` / `heavy_sound`). Embedded as
  `VehicleDesc::crash_sound`, mirroring the existing `VehicleDamageDesc`
  pattern (same struct, parsed the same way, editable the same way).
- `game::VehicleCrashSound` (new): owns three `SoundEffectComponent`
  instances (one per severity tier) and a debounce cooldown timer.
  `RegisterImpact(world_point, impulse)` classifies the impulse into a
  tier (`ClassifyTier`), computes a gain multiplier proportional to
  impulse (`ComputeGain`, saturating at `max_impulse`), triggers the
  matching component, and arms the cooldown. `Update(dt)` ticks the
  cooldown down each frame. No-op below `min_impulse` or while debounced.
- `game::SoundEffectComponent::Trigger` gained an optional `gain_scale`
  parameter (default `1.0`, backward compatible) so a single component can
  have its volume scaled per-call instead of being fixed at construction —
  needed to make one "light crash" sample play quieter for a glancing hit
  and louder for a harder one.
- `GameVehicle` now owns a `VehicleCrashSound crash_sound_`, constructed
  from the template's `VehicleDesc::crash_sound` plus optional
  `audio::SoundManager*` / `audio::ResourceManager*` passed into the
  constructor (both default to `nullptr`, matching `SoundEffectComponent`'s
  own null-safe contract). `OnCollision` forwards every raw impact to
  `crash_sound_->RegisterImpact(...)` in addition to the existing
  `damage_->RegisterImpact(...)` call — crash sound and damage are
  independent listeners on the same raw signal, not chained. `Update(dt)`
  ticks `crash_sound_` alongside the rest of the per-frame vehicle state.
- `VehicleTemplate` parses a new `crash_sound:` YAML section (parallel to
  `damage:`) with the same "ignore if missing, fields default individually"
  behavior as the rest of `VehicleDesc`'s parsing.
- `MapLoader::ParseVehicle` now threads the audio managers already
  available in `MapLoader::Load` (used for `sound_emitter` objects) into
  `GameVehicle`, so vehicles placed via a `.map.yaml` get working crash
  sounds automatically, no separate opt-in required.
- `VehicleEditorWindow` gained a "Crash Sound" section (after "Damage"):
  three sound pickers (reusing `SoundEmitterSelectionModal`, one per tier)
  plus `DragFloat` controls for the four impulse thresholds and the
  debounce time, with load/save wired into the existing YAML round-trip.

## Decisions and rationale

- **Independent of `VehicleDamage`, not built on top of it.** The issue
  explicitly scopes this as its own small feature that "consumes the
  impulse plumbing" from `VehicleDamage` rather than being folded into it.
  `IVehicleDamageListener::OnDamageThresholdCrossed` only fires when a
  *damage* threshold (HP-fraction based) is crossed — a light tap that
  never dents a zone would never reach it, but the acceptance criteria
  explicitly want *some* audio feedback (or none) down to a raw-impulse
  floor independent of HP state. So `VehicleCrashSound` taps
  `GameVehicle::OnCollision` directly, the same raw signal `VehicleDamage`
  taps, rather than subscribing as a damage listener.
- **Debounce is a plain cooldown timer, not a state machine.** A single
  sustained contact (e.g. scraping a wall) fires many `OnContactAdded`
  events in quick succession; `desc.debounce_time` (default 0.2s) is the
  minimum gap between two triggered one-shots, armed unconditionally on
  every successful trigger regardless of tier. This is enough to satisfy
  "no machine-gun repetition" without needing per-tier cooldowns or a
  smoothing/rolling-average model, which would be premature complexity for
  a one-shot sound bank.
- **Gain scaling reuses `SoundEffectComponent` via a new optional
  parameter rather than a new mechanism.** The issue calls out reusing the
  existing one-shot mechanism specifically. `SoundEffectComponent::Trigger`
  previously had a fixed gain baked in at construction; adding an optional
  `gain_scale` (default `1.0`, so every other call site is unaffected) was
  the smallest change that let one component's volume vary per hit instead
  of requiring N pre-baked volume variants per tier.
- **Tier selection and gain are exposed as static, pure functions
  (`ClassifyTier` / `ComputeGain`) for testability.** Neither
  `SoundEffectComponent` nor the wider `audio` module has any existing unit
  test coverage in this codebase (confirmed before starting: zero call
  sites, zero test files) — triggering a real one-shot requires a live
  `ISoundSystem` backend and on-disk `.sound.yaml` assets, which is out of
  scope for a unit test. Rather than introduce a new sink/mock interface
  (abstraction the issue doesn't ask for), the classification and gain math
  — the actually interesting logic — is static and audio-free, and
  `VehicleCrashSoundTest.cpp` covers it directly (tier boundaries, gain
  proportionality/saturation, debounce arming/expiry/non-rearm) the same
  way `VehicleDamageTest.cpp` covers `VehicleDamage`'s math without
  touching physics.
- **`CrashSoundDesc` defaults are calibrated against `VehicleDamageDesc`'s
  defaults**, not picked arbitrarily: with the stock
  `impulse_to_damage_scale = 0.05` and `zone_max_hp = 100`, crossing the
  first damage threshold (0.4) takes an impulse of ~800. `min_impulse =
  150` / `medium_impulse = 500` / `heavy_impulse = 1200` place a "moderate
  crash" (~800 impulse, first damage threshold) solidly in the
  medium-to-heavy range, so the two systems feel consistent out of the box
  even though they're independently configured.
- **Editor Play Mode is wired up too, added after initial review.** The
  first pass of this feature left `PlayModeManager::Enter` constructing
  `GameVehicle` with null audio managers, reasoning that `MapLoader`
  (the actual game app) was the primary consumer and editor Play Mode was
  out of scope. In practice, Play Mode is the *only* interactive way to
  collide a vehicle in the editor, so shipping without it meant the
  feature was untestable by ear immediately after merge — a real gap, not
  a reasonable deferral. Fixed by threading `EditorWindow`'s
  `editor_sound_manager_` / `editor_sound_resources_` (gated by
  `toolbar_->IsSoundEnabled()`, the same pattern already used for
  `GameSoundEmitter` placement) through a new
  `PlayModeManager::Enter(vehicle_name, sound_manager, resource_manager)`
  overload. Both new parameters default to `nullptr` so no other caller
  needed to change.

## Output to keep in mind for future work

- `CollisionEstimationResult` (per `PhysicsSystem.cpp`'s contact listener)
  currently only surfaces a scalar impulse and an averaged world-space
  point — no contact normal, no other-body reference. If crash sounds ever
  need to distinguish "direction of impact" (e.g. panning, or picking a
  different sample for a rear-end vs. a side hit), that will require
  extending `IPhysicsCollisionListener::OnCollision` itself, which today
  only the `VehicleDamage`/`VehicleCrashSound` consumers would need to
  adapt to.

## Skills used

- `impl-issue`

## CLAUDE.md / skill instructions specifically followed

- `src/CLAUDE.md`: one class per `.h`/`.cpp`, project-relative includes,
  new module source registered in `src/CMakeLists.txt`/`game/CMakeLists.txt`.
- `src/game/CLAUDE.md`: `VehicleCrashSound` is a plain component (not a
  `GameObject`), matching the documented "components vs. scene objects"
  distinction already established for `SoundEffectComponent`.
- `src/physics/CLAUDE.md`: `CrashSoundDesc` stays Jolt-free (only
  `core::Vec3f`/floats/`std::string`), consistent with the rest of
  `VehicleDesc`.
- `src/editor/CLAUDE.md`: `VehicleEditorWindow`'s new section is pure UI
  (reads/writes `vehicle_desc_.crash_sound` and calls
  `SoundEmitterSelectionModal::Open()`/`Render()`); no editing logic lives
  in the panel. Also followed the "value members require a full `#include`"
  rule for the three new `SoundEmitterSelectionModal` members.
- Root `CLAUDE.md`: branch prefixed `feat/`, based on latest `dev`,
  `cpplint`/`cppcheck` run clean before commit, conventional-commit message,
  PR opened against `dev` with a closing keyword for #829.

## Propositions

- No build/install process changes — no README update needed.
- No `CLAUDE.md` or specification gaps surfaced during this task; the
  `VehicleDamage` precedent (impulse plumbing, `VehicleDesc` sub-struct
  pattern, editor section pattern) covered every question this issue raised.
