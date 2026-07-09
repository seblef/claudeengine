# TerrainTile: surface overlay (oil slicks, ramps)

Implements #836: a flat surface-overlay quad placed at terrain height, with
its own material and a physics friction/restitution override under its
footprint — the simplest possible hazard primitive (oil slicks, mud, ramps)
without touching the terrain mesh itself.

## What changed

- **`track::TileDesc`** (`src/track/TileDesc.h`, header-only): plain data —
  `width`, `length`, and a `physics::PhysicsMaterialDesc surface` override.
- **`track::TerrainTile`** (`src/track/TerrainTile.h/.cpp`): a static
  `BuildQuad(width, length)` helper, mirroring `track::RoadMeshBuilder`. Builds
  a flat 4-vertex/2-triangle quad in local space (XZ plane, +Y normal). The
  quad deliberately does **not** follow terrain curvature — per the issue,
  this is a surface-property override under a flat pad, not a terrain
  modification, so it is posed flat by the owning GameObject's world
  transform and snapped once to terrain height at placement time.
- **`game::GameTerrainTile`** (`src/game/GameTerrainTile.h/.cpp`): the actual
  scene object (`GameObject` subclass), modeled directly on `game::GameRoad`
  — owns the render mesh/instance and a static physics body, rebuilt via
  `RegenerateMesh()`. New `GameObjectType::kTerrainTile` and a
  `GameObjectVisitor::Visit(GameTerrainTile&)` overload (implemented in
  `MapSerializer::SerializeVisitor`; no-op in `PlayModeManager::EnterVisitor`,
  matching `GameRoad`'s treatment there).
- **Editor integration**: `EditorTool::kCreateTerrainTile` toolbar button
  (oil-can icon), wired through the existing generic `PlacementTool` exactly
  like `GamePivot`/`GamePlayerStart` (no dedicated tool class needed — the
  footprint is a fixed quad, not an edited spline like `RoadTool`).
  `PropertiesPanel::RenderTerrainTileProperties` exposes width/length/
  friction/restitution sliders (mutate + regenerate immediately, same
  no-undo pattern as `GameRoad`'s width slider) and a material picker that
  pushes a new `TerrainTileMaterialAssignCommand` (undoable, mirrors
  `RoadMaterialAssignCommand`/`MaterialAssignCommand`). `MaterialEditorWindow
  ::ApplyToSelection` gained a `kTerrainTile` branch for consistency with the
  existing `kRoad` special case.
- **`.map.yaml` round-trip**: `MapSerializer::SerializeVisitor::Visit
  (GameTerrainTile&)` emits `type: terrain_tile`, `transform`, `width`,
  `length`, `friction`/`restitution` (only when non-default, reusing the same
  omit-if-default convention as `EmitPhysicsBodyDesc`), and `material`.
  `game::MapLoader::ParseTerrainTile` parses it back and calls
  `RegenerateMesh()` immediately — no third-pass terrain-draping step is
  needed (unlike `GameRoad`), since the tile is flat and its Y is already
  baked into the serialized `transform`.
- **Physics — friction (no new mechanism needed)**: the tile's own static
  physics body is a thin box (`PhysicsShapeDesc::MakeBox`, half-height 0.1 m,
  `center_offset` lifting it so its top face clears typical terrain variation
  across the footprint) created via `PhysicsSystem::CreateBody()` with
  `PhysicsBodyDesc::material = TileDesc::surface`. Since `ApplyMaterialAndLayer`
  already copies `material.friction`/`restitution` onto the Jolt body, and
  Jolt's default per-wheel friction combine is `sqrt(tireFriction *
  bodyFriction)`, a low-friction tile body naturally reduces wheel grip the
  moment the wheel raycast hits it — **no `PhysicsSystem` API changes were
  needed for the oil-slick case.**
- **Physics — restitution / ramp launch (new mechanism)**: wheeled vehicles
  drive via raycast + suspension spring
  (`JPH::VehicleCollisionTesterRay`, see `PhysicsSystem::CreateVehicle`), so
  Jolt's rigid-body restitution response is never consulted at the wheel/
  ground contact — a high-`restitution` tile body would otherwise do nothing.
  Added `PhysicsSystem::ApplyWheelRestitution()` (called at the end of
  `Step()`): for every wheel in contact, reads the contact body's restitution
  via `JPH::BodyInterface::GetRestitution(wheel->GetContactBodyID())`, and if
  the wheel is compressing into the surface (`velocity·normal < 0`), applies
  an extra impulse along the contact normal approximating a restitution
  bounce (`-(1+e) * v_n * mass / 4`, split across the 4 wheels). This is a
  general mechanism — not tied to `TerrainTile` specifically — so any body
  with elevated restitution (not just tiles) now gives vehicles a bounce.

## Decisions & rationale

- **File-location deviation from the issue text.** The issue names
  `track/TerrainTile.h/.cpp` as "the scene object." Following that literally
  would require `TerrainTile` to derive from `game::GameObject`, but
  `GameObject` lives in `game/`, and `track/` depends only on
  `core, abstract, physics` (never `game/` — confirmed in
  `src/track/CMakeLists.txt`; `game` depends on `track`, not the reverse).
  Rather than invert that dependency, this mirrors the existing
  `RoadSpline`/`RoadMeshBuilder` (geometry, in `track/`) vs. `GameRoad` (scene
  object, in `game/`) split: `track::TerrainTile` is a pure geometry helper,
  `game::GameTerrainTile` is the actual scene object. Same two files the
  issue asked for, functionally reassigned to match how the rest of the
  codebase already draws this line.
- **No `SurfaceOverrideRegion`/registry in `PhysicsSystem`.** An earlier draft
  of this change added a full AABB-tested override-region registry queried
  from a custom `VehicleConstraint::SetCombineFriction` callback. Once it was
  clear the tile's own physics body already carries the right
  `friction`/`restitution` as native Jolt body properties, and that the wheel
  raycast hits that body directly, Jolt's *default* combine function already
  produces the desired oil-slick behaviour with zero new physics-system
  surface area. Simpler and there's less to review/maintain.
- **Flat quad, no terrain draping.** The issue explicitly frames this as "no
  terrain modification, just a surface-property override under a quad," and
  WRECKONING.md §11b says "a rectangle placed... at terrain height" (singular
  height, not per-vertex). A single flat quad following `RoadMeshBuilder`'s
  more complex per-vertex `height_fn` drape would be over-engineering for
  what's meant to be the simplest hazard primitive in the game.
- **Physics collider thickness (0.1 m half-height, offset upward).** Wheel
  raycasts must hit the tile's thin box before reaching the terrain
  heightfield underneath (which sits at roughly the same height). A purely
  coincident quad-thin box risked the raycast hitting whichever shape Jolt's
  broadphase happened to sort first. Lifting the box (`center_offset`) so its
  top face is comfortably above local terrain variation removes that
  ambiguity; there is no visual difference since the box isn't rendered
  (only the paper-thin render quad is).
- **No undo for width/length/friction/restitution sliders**, only for
  material assignment. This exactly matches `GameRoad`'s existing precedent
  (`RenderRoadProperties`'s width slider isn't undoable either); introducing
  a different UX convention just for this one panel would be inconsistent
  for no real benefit.
- **`TerrainTileMaterialAssignCommand`** is a near-duplicate of
  `RoadMaterialAssignCommand`/`MaterialAssignCommand`. Not collapsed into one
  generic template/command, because that's the existing convention (a
  distinct tiny command class per object type) — see `src/editor/commands/`.
- **Outliner/ObjectsPanel icon grouping left untouched.** `GameObjectType::
  kRoad` isn't in either panel's icon switch or `RenderTypeGroup` list despite
  being a shipped, complete feature; `kTerrainTile` was left at the same
  level of polish for consistency rather than fixing a pre-existing gap that
  wasn't part of this issue's scope.

## Verification

- `cmake --build _build --target wreckoning wreckoning_editor -j$(nproc)`:
  clean full build (including a from-scratch `cmake -S . -B _build
  -DBUILD_EDITOR=ON` reconfigure).
- `cpplint` on every new/changed file: no warnings.
- `ctest`: all 53 project tests pass, including 3 new
  `tests/track/TerrainTileTest.cpp` cases covering `BuildQuad`'s vertex
  count, local-space extents, and index bounds (new `tests/track/`
  directory + `add_subdirectory(track)` in `tests/CMakeLists.txt`, following
  `tests/game/CMakeLists.txt`'s pattern of compiling the tested `.cpp`
  directly into the test binary rather than linking the full static lib).
- **Not verified in this environment (headless, no display — see root
  `CLAUDE.md`)**: actually placing a tile in the editor viewport, confirming
  the materialed quad renders and conforms to terrain height, and driving a
  vehicle over an oil-slick/ramp tile to feel the grip/launch behaviour.
  These are flagged as required follow-up manual/visual QA before this
  feature is considered fully done, in particular:
  - Whether `kColliderHalfHeight = 0.1f` is the right collider thickness for
    typical terrain roughness (may need tuning per-map).
  - Whether the `ApplyWheelRestitution` bounce impulse magnitude
    (`-(1+e) * v_n * mass / 4`) reads as a satisfying "ramp launch" in
    practice, or needs a tunable multiplier exposed on `TileDesc`.

## Follow-ups / open questions for next contribution

- The gated test-track data issue should add at least one oil-slick and one
  ramp `terrain_tile` entry to a test map, plus manual driving verification
  of both behaviours once a display is available.
- Consider exposing a restitution-impulse multiplier on `TileDesc` if manual
  QA finds the default launch feel needs tuning per-tile rather than only
  via the raw Jolt restitution coefficient.
- `ObjectsPanel`/`OutlinerPanel` grouping/icons don't cover `kRoad` or
  `kTerrainTile`; worth a small follow-up pass across both once another
  "misc scene object" type lands, rather than one-off patching each time.

## Skills / instructions consulted

- Skill used: `impl-issue` (this contribution's entry point).
- `CLAUDE.md` (root): git workflow (branch from `dev`, conventional commit,
  PR to `dev`), "do not launch windowed binaries in this headless
  environment — rely on cpplint/cppcheck/ctest and flag visual QA as
  follow-up," and the history-file requirement this document fulfills.
- `src/CLAUDE.md`: one class per `.h`/`.cpp`, Google C++ style, project-
  relative includes, new-module CMake wiring.
- `src/game/CLAUDE.md`: `GameObject` subclass vs. component distinction
  (confirmed `GameTerrainTile` is a scene object — has map presence, editor
  placement, its own renderer/physics resources), `OnAddedToScene`/
  `OnRemovedFromScene`/`OnWorldTransformUpdated` invariants.
- `src/editor/CLAUDE.md`: GUI/edit-logic separation (kept
  `RenderTerrainTileProperties` as pure UI, no algorithm inline beyond
  trivial slider-to-struct assignment); "prefer `std::find_if` over raw
  index loops" (not applicable here — no new index loops added).
- `src/physics/CLAUDE.md`: Jolt types must never leak into public
  `physics/*.h` — `ApplyWheelRestitution` and all `JPH::Wheel`/
  `JPH::BodyInterface` usage stay entirely inside `PhysicsSystem.cpp`.
