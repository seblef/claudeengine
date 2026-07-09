# Vehicle Damage Meshes

Implements GitHub issue #834: visual damage mesh swaps (pristine → dented →
crumpled → wrecked) and the Vehicle editor panel's **Damage** tab to author
them. Per `WRECKONING.md` §6 (visual degradation) and §14b (Vehicle panel —
Milestone 2 damage-meshes tab).

## Key design decision, discussed with the user before implementing

The issue's editor spec reads "per zone (Front, Rear, Left, Right, Roof): a
list of mesh variants at configurable HP thresholds" — read literally, that
implies five independently-authored mesh lists. But `GameVehicle` has exactly
**one** visible `body_mesh_` (`std::unique_ptr<GameMesh>`), not a sub-mesh per
zone — there is nothing for five independent lists to each drive.

Two options were discussed with the user:
1. Keep one body mesh; pick which zone's variant "wins" by whichever zone is
   currently *most* damaged ("worst zone").
2. Keep one body mesh; average the five zones' damage fractions into a single
   scalar and use *that* to pick the active variant.

The user picked averaging, specifically to avoid a flicker/discrepancy
failure mode of "worst zone": if two zones are neck-and-neck (e.g. front at
61%, left at 59%), a single additional hit anywhere can flip which zone is
"worst," snapping the visible mesh to an unrelated zone's variant even though
overall damage barely changed. Averaging is monotonic and continuous instead.

Once averaging was chosen it followed that there can only be **one shared
list** of body-mesh variants, not five: averaging collapses five fractions
into one scalar, and there is no principled way to pick "whose" list's mesh to
show for that scalar. So the final schema is:
- Zone HP/thresholds stay genuinely per-zone (already existed).
- Gameplay effects (speed/steering reduction) are driven by the **front**
  zone's fraction only, matching `WRECKONING.md` §6's explicit precedent
  ("40% front → reduced steering", "60% engine → reduced max speed" — front
  stands in for "engine" per the existing `VehicleDamage` history doc, since
  no "engine" zone exists).
- Body-mesh variants are a **single shared list** (not per zone), each row a
  `{threshold, mesh_path}` pair, selected by the *average* damage fraction
  across all five zones.

## Summary of changes

### `physics::VehicleDesc.h` — new schema

- `DamageMeshVariant` (new): `{float threshold; std::string mesh_path;}`.
- `VehicleDamageEffectsDesc` (new): parallel `std::array<float,4>`/
  `std::array<bool,4>` pairs (`steering_scale`/`steering_enabled`,
  `speed_scale`/`speed_enabled`), indexed the same as the existing
  `VehicleDamageDesc::thresholds`. `enabled[i]` gates whether threshold `i`
  has any effect at all (so a threshold can exist without being wired to an
  effect); when several enabled thresholds are crossed, the highest one wins
  — effects do not stack multiplicatively.
- `VehicleDamageDesc` gained `mesh_variants` (`std::vector<DamageMeshVariant>`)
  and `effects` (`VehicleDamageEffectsDesc`).

### `game::VehicleDamage` — averaging accessor

Added `GetAverageDamageFraction()`: mean of `GetDamageFraction()` across all
five zones. This is the only new public surface `VehicleDamage` needed; it
stays a pure notifier otherwise (per its existing class-comment invariant).

### `game::GameMesh::SetTemplate()` (new)

The mesh-swap primitive: releases the old template, `AddRef`s the new one,
rebuilds the `MeshInstance` (preserving the current world transform), updates
the local bbox via the existing protected `SetLocalBBox()`, and
re-registers with the renderer if the mesh is currently in scene and visible.
Physics bodies are untouched — the body mesh this is used on never has one
(vehicle physics runs on the parent `GameVehicle`), so no rebuild path was
needed there.

### `game::VehicleTemplate` — preloading damage-mesh variants

`ParseDamageDesc()` gained parsing for `mesh_variants` (a YAML sequence of
`{threshold, mesh}`) and `effects` (four parallel scale/enabled arrays).
After the body/wheel templates load successfully, each variant's mesh is
preloaded once via `MeshTemplate::GetOrLoad()` (per `game/CLAUDE.md`'s "never
`new MeshTemplate()` directly" rule) into a new
`body_damage_variant_tmpls_` vector, index-aligned with
`vehicle_desc_.damage.mesh_variants`. A variant with an empty path, or whose
mesh fails to load, gets a `nullptr` slot (logged as a warning) rather than
failing the whole vehicle load — matches the tolerant-degradation pattern
used for sound assets elsewhere in this file. New accessors:
`GetBodyDamageVariantCount()` / `GetBodyDamageVariantTemplate(index)`.

### `game::GameVehicle` — runtime swap + effects, both polled per-`Update()`

Two new private methods, both called every `Update()` tick while
`physics_vehicle_` is active (alongside the existing `crash_sound_`/
`scrape_listener_` updates):

- `UpdateBodyMeshVariant()`: reads `damage_->GetAverageDamageFraction()`,
  finds the authored variant with the highest threshold at or below that
  fraction (falling back to the pristine `GetBodyTemplate()` if none
  qualifies), and calls `body_mesh_->SetTemplate()` only when the resolved
  variant actually changed (`current_mesh_variant_` tracks the last-applied
  index so this is a no-op most frames).
- `UpdateDamageEffects()`: reads the **front** zone's fraction and computes
  `damage_steer_scale_` / `damage_speed_scale_` via a small
  `ComputeDamageEffectScale()` helper (returns the scale of the highest
  enabled threshold at or below the fraction, or `1.0` if none apply).
  These multiply into the existing steer-scale computation
  (`ComputeSteerScale(speed, desc) * damage_steer_scale_`) and into every
  `SetThrottle()` call (forward and reverse), so a damaged front end
  measurably saps both handling and acceleration once an authored threshold
  is crossed and enabled.

**Deliberately not routed through `OnDamageThresholdCrossed()`/the Jolt-
callback-deferral machinery** that the wreck-state issue (#833) had to build:
mesh swaps and effect-scale recomputation are pure per-frame polling in
`Update()`, which already runs outside any Jolt contact callback (same
guarantee the wreck deferred-notification fix relies on) — no new
reentrancy hazard, and no need for a `_pending`-style flag since nothing here
is one-shot.

**Coordination with the wreck-state issue (#833)**: that issue's history doc
explicitly flagged "whoever implements progressive mesh swapping should hook
the 100% case into the same `OnDamageThresholdCrossed()` wreck branch." This
implementation does *not* do that — the 100%/wrecked mesh is just the
highest-threshold entry in `mesh_variants` like any other, picked up
automatically by the same per-frame `UpdateBodyMeshVariant()` poll once the
average fraction reaches it, with no special-casing needed. Kept independent
of `drive_state_ == kWrecked`/`OnDamageThresholdCrossed()` on purpose: a
single zone reaching 100% flips `drive_state_` to wrecked, but the *average*
across all five zones reaching the wrecked-mesh's threshold is a separate,
independently-tunable condition (per the chosen averaging design).

### `editor::VehicleEditorWindow` — Damage tab

The issue's editor spec describes `editor/VehiclePanel.h/.cpp` as an
`IResourcePanel`; that class doesn't exist in this codebase today — the
actual implementation is `VehicleEditorWindow`, a standalone floating window
wired into `ResourcePanelRegistry` via its external-open-callback path (not
the `IResourcePanel` factory path other panels use). Targeted that existing
class rather than starting a parallel `VehiclePanel`/`IResourcePanel`
migration, which would be unrelated scope creep.

- **Tab bar** (new): `Render()` now wraps the existing sections in
  `ImGui::BeginTabBar`/`BeginTabItem` (pattern copied from
  `TerrainEditorPanel::Render()`, the only other panel already using tabs) —
  a **Vehicle** tab (Body, Wheels, Physics — unchanged) and a **Damage** tab
  (zone HP/thresholds, the new Gameplay Effects section, the new Body Damage
  Meshes section, then Crash Sound/Scrape/Fire/Wreck, since those are all
  damage-and-destruction-adjacent systems). `DrawActionsBar()` stays outside
  the tab bar, shared by both tabs.
- **`DrawGameplayEffectsSection()` (new)**: one row per existing
  `damage.thresholds` entry (read-only percentage label — thresholds
  themselves are still edited once, in `DrawDamageSection()`, to avoid two
  widgets editing the same array), each row a Steer checkbox+slider and a
  Speed checkbox+slider (disabled via `ImGui::BeginDisabled()` when
  unchecked).
- **`DrawBodyMeshVariantsSection()` (new)**: a variable-length list of
  `{threshold, mesh_path}` rows — `SliderFloat` threshold, mesh path label +
  "Browse..." (reuses the `PickBodyMesh()` NFD-dialog pattern) + "Remove", and
  an inline 96×96 `editor::MeshPreview` thumbnail per row once a mesh path is
  set (kept small since each `MeshPreview` owns its own FBO/`PreviewRenderer`/
  GBuffer — instantiating one per row is not free). "Add Mesh Variant"
  appends an empty row. Two parallel index-aligned vectors track this:
  `mesh_variant_tmpls_` (ref-counted `MeshTemplate*`, released on
  remove/rebuild/destruction) and `mesh_variant_previews_`
  (`unique_ptr<MeshPreview>`, lazily constructed only once a mesh path is
  set — an added/empty row has a `nullptr` preview until "Browse..." is used).
- **YAML round-trip**: `LoadFromYaml()`/`SaveToYaml()` extended for
  `damage.mesh_variants` (a sequence of `{threshold, mesh}`) and
  `damage.effects` (four flow-sequence arrays), mirroring
  `VehicleTemplate.cpp`'s independent parser (this file already duplicates
  that parsing on purpose, per the pre-existing pattern noted in the
  `VehicleDamage` history doc). `RebuildMeshVariantPreviews()` re-syncs the
  two preview-tracking vectors after every load (including Revert, which
  calls `LoadFromYaml()` directly without going through `Open()`).

## Follow-up: no damaged mesh assets exist yet

`data/vehicles/hell_sized.vehicle.yaml` was **not** updated with an example
`mesh_variants`/`effects` block — `data/meshes/vehicles/` only contains the
pristine `duke_car_body(_sized).emesh`; no dented/crumpled/wrecked variant
meshes have been authored yet. Both new fields default to empty/disabled
(matching the shipped behaviour with zero mesh_variants and all
`*_enabled = false`), so this is a no-op until art authors real variant
meshes and adds them via the new Damage tab — the runtime/editor plumbing is
ready to consume them the moment they exist.

## Tests

- `tests/game/VehicleDamageTest.cpp`: 4 new cases for
  `GetAverageDamageFraction()` — pristine (0), single damaged zone (fraction
  / 5), two damaged zones (sum / 5), and all five zones fully destroyed (1).
  19/19 `VehicleDamageTest` cases pass (was 15).
- No new `GameVehicleTest.cpp` / `VehicleTemplateTest.cpp` — confirmed (as
  the prior Wreck State history doc also found) neither `GameVehicle` nor
  `VehicleTemplate` is unit-tested anywhere in this codebase today, since
  exercising either needs `PhysicsSystem`/`Renderer`/mesh loading standing
  up. The new logic in both (`UpdateBodyMeshVariant()`,
  `UpdateDamageEffects()`, `ComputeDamageEffectScale()`, variant preloading)
  was covered by code reading + the full build/cpplint/cppcheck pass instead,
  consistent with this codebase's existing test-coverage boundary.

## Verification performed

- `cpplint` clean on every new/changed file.
- `cppcheck` clean using the **exact** invocation the repo's
  `.hooks/pre-commit` script runs (`--enable=all --inline-suppr
  --suppress=missingIncludeSystem,unusedFunction,unmatchedSuppression,
  normalCheckLevelMaxBranches,checkersReport --error-exitcode=1`) — note a
  simpler ad-hoc `--enable=warning,style,performance` invocation (no
  `--inline-suppr`) falsely flags `unusedStructMember` on every composite-
  typed struct field (confirmed this false positive already exists
  identically on unmodified `dev` for pre-existing fields like
  `VehicleDesc::damage`/`crash_sound` — a quirk of that flag combination, not
  a real issue; the pre-commit hook's actual invocation doesn't have it).
- Full rebuild: `game`, `editor`, `wreckoning`, `wreckoning_editor`,
  `game_tests` — all succeed with no warnings from the new/changed code.
- `game_tests` 19/19 (`VehicleDamageTest`) run directly; full `ctest` run
  82% pass, with all 70 failures being pre-existing "Not Run" third-party
  `gli`/`glm` sampler-test binaries unrelated to this change (same recurring
  caveat prior VFX/vehicle history docs in this repo have noted for this
  no-GL-context environment).
- **Not done**: no live in-game/editor visual confirmation — this
  environment is headless (per root `CLAUDE.md`, `wreckoning`/
  `wreckoning_editor` must not be launched here). Flag for manual QA once
  damaged mesh assets exist: author a dented/crumpled/wrecked variant set for
  a real vehicle, wire them via the new Damage tab's "Add Mesh Variant" +
  "Browse..." buttons, confirm the inline thumbnails render, save, then drive
  that vehicle into repeated collisions and confirm the body mesh visibly
  progresses through the variants as damage accumulates, ending at the
  authored 100%-threshold ("wrecked") mesh: confirm steering feels
  measurably reduced once an enabled steering threshold is crossed, and
  throttle response/top speed measurably drops once an enabled speed
  threshold is crossed.

## Output to keep in mind for next Milestone 3 issues

- The wreck-state issue's (#833) explosion/disabled-vehicle payoff and this
  issue's mesh swap are intentionally independent: a single zone hitting
  100% wrecks the vehicle (control cutoff, wheels freeze) regardless of what
  the *average* fraction is, while the visible body mesh is driven purely by
  the average. In the common case where thresholds are authored sensibly
  (e.g. the highest mesh-variant threshold ≈ 1.0), both will resolve near
  the same moment, but a vehicle can technically wreck via one zone spiking
  to 100% while the average — and therefore the displayed mesh — is still
  mid-progression. If this reads as a bug in playtesting, the fix is to
  author the final `mesh_variants` entry at a threshold at or below what
  the "typical" wreck-triggering average looks like, not a code change.
- `GameVehicle::current_mesh_variant_` starts at `-1` (pristine) and is only
  ever written by `UpdateBodyMeshVariant()`, so `Copy()`-cloned vehicles
  (e.g. respawns) correctly start pristine even if the template's default
  damage state were ever non-zero (it currently always is zero at
  construction, so this is a latent-safety note more than an active bug).
- `DamageMeshVariant::threshold` values need not be authored in ascending
  order — both `VehicleEditorWindow` and `GameVehicle` resolve "the highest
  threshold at or below the current fraction" by scanning the whole list, not
  by assuming sortedness — so reordering rows in the editor (there's no
  explicit sort/reorder UI yet) doesn't require the user to also re-enter
  them in order.

## Propositions

**CLAUDE.md improvements**: none — the existing `game/CLAUDE.md` and
`editor/CLAUDE.md` guidance (MeshTemplate::GetOrLoad() convention, GUI/logic
separation, value-member full-include rule) already covered every judgment
call this issue required without needing clarification mid-implementation.

**README.md**: no build/install process changes (no new third-party library,
no new CMake option) — no proposal needed.

**Specification questions**: one real judgment call, raised with the user
before implementing (see "Key design decision" above) — whether the vehicle
needs five independently-authored mesh lists (implying new per-zone
sub-meshes, a bigger architecture change) or one shared list driven by a
single scalar derived from all five zones, and if the latter, worst-zone vs.
average. Resolved in favour of the smaller-scope single-body-mesh design with
averaging, per the user's flicker-avoidance reasoning.

## Skills used

- `impl-issue` (invoked via `/impl-issue 834`).

## CLAUDE.md instructions especially taken into account

- Root `CLAUDE.md` git workflow: checked out and pulled `dev` first, branched
  `feat/vehicle-damage-meshes-834`, ran `cpplint`/`cppcheck`, conventional-
  commit message, PR to `dev` with closing keyword.
- Root `CLAUDE.md` verification guidance: did not attempt to launch
  `wreckoning`/`wreckoning_editor` (headless environment); relied on
  `cpplint`, `cppcheck`, the `game_tests` binary, and full rebuilds of
  `wreckoning`/`wreckoning_editor` instead, and explicitly flagged the
  visual/manual-QA gap above (compounded here by the missing art assets).
- `src/game/CLAUDE.md`: `MeshTemplate::GetOrLoad()` (never `new
  MeshTemplate()` directly) followed for every damage-variant mesh load;
  mesh-swap logic stayed inside the existing `GameVehicle`/`GameMesh` scene
  objects rather than introducing a new `GameObject` subclass, since it's
  per-frame state mutation on an existing object, not a new scene entity.
- `src/editor/CLAUDE.md`: GUI/logic separation — `DrawGameplayEffectsSection()`
  / `DrawBodyMeshVariantsSection()` stay pure UI (read/write
  `vehicle_desc_.damage` fields via ImGui controls, delegate mesh loading to
  `UpdateMeshVariantPreview()`/`RebuildMeshVariantPreviews()` rather than
  inlining `MeshTemplate::GetOrLoad()` calls in the draw functions); value
  members needing a full include satisfied via a forward-declared
  `editor::MeshPreview` (pointer/`unique_ptr` members only need the forward
  declaration since the destructor — where the type must be complete — is
  already defined out-of-line in the `.cpp`).
- `src/physics/CLAUDE.md`: new `DamageMeshVariant`/`VehicleDamageEffectsDesc`
  structs stay Jolt-free (plain `float`/`bool`/`std::string`/`std::array`/
  `std::vector`), consistent with every existing `*Desc` struct.
