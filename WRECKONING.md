# Wreckoning — Design & Roadmap

*A modern vehicular combat game inspired by Carmageddon (Stainless Games, 1997).*

## Vision

**Wreckoning** is a vehicular combat game set in semi-open 3D environments. The player drives a heavily armed car and wins by either:

- completing all race checkpoints in order, or
- wrecking every opponent car, or
- running over every pedestrian on the map.

Time extensions are earned through destruction. Power-ups are scattered around the map. Opponents have simple but aggressive AI. Crashes cause progressive vehicle damage. The tone is dark, carnage-focused, and arcadey.

This document maps the current engine state against what the game needs, identifies the gaps, and lays out a phased implementation roadmap.

---

## Global Design Principles

### Editor-first authoring

**Every game resource must be creatable and editable inside the editor.** No resource requires hand-writing YAML or running an external tool. This applies to vehicles, VFX effects, pedestrians, power-up pickups, race configs, and road splines alike.

This principle has two concrete implications:

1. **All resource descriptors are YAML files.** `VehicleDesc`, `VFXExplosionDesc`, `PedestrianDesc`, etc. each serialise fully to/from YAML. The file is the source of truth; the editor reads and writes it.
2. **The editor provides a dedicated inspector panel for each resource type.** Panels are registered per type; the editor dispatches to the right one when a resource file is opened. Changes reflect live in the 3D viewport — no Play required to see a wheel move or a particle colour change.

The in-scene map editor (placing and transforming scene objects) and the resource editor (authoring the descriptor files those objects reference) are complementary: a `GameVehicle` in the scene points to a `VehicleDesc.yaml`; editing the scene positions the vehicle, editing the descriptor defines what the vehicle is.

### Zero resource reload on Play

See Section 13. In-memory resources are shared between editor and game systems — no reload, no re-upload on mode transition.

---

## Current Engine State

### What exists

| Module | Status | Notes |
|---|---|---|
| `renderer` | Solid | Deferred lighting, GBuffer, shadow maps (CSM + omni), bloom, eye adaptation, particles, foliage, trees |
| `physics` | Partial | Jolt Physics wrapper: rigid bodies, static meshes, raycasting, `CharacterController`. No vehicle constraint yet. |
| `audio` | Solid | `SoundManager`, 3D positional audio, `SoundEffectComponent` for one-shot triggers |
| `game` | Partial | `GameSystem` loop, `GameObject` hierarchy, `MapLoader`, `GameTerrain`, `FPSCameraController` only |
| `ui` | Solid | `UIRenderer`, sprites, text, font atlas, `HUDScreen`, `LoadingScreen` |
| `terrain` | Solid | Heightmap terrain with foliage — good for race environments |
| `editor` | Solid | In-game world editor with selection, transform, and material tools |
| `particles` | Solid | GPU particle systems — ready for sparks, smoke, debris, blood |

### Dependency graph (current)

```
app → game → renderer → abstract → core
              ├── physics → core
              ├── audio → core
              ├── ui → abstract → core
              ├── terrain → renderer
              └── particles → renderer
```

---

## Gap Analysis

The table below lists every major system the game needs and its current availability.

| System | Available | Gap |
|---|---|---|
| Vehicle physics (wheel, suspension, torque, braking) | No | New `PhysicsVehicle` in `physics/` using Jolt `VehicleConstraint` |
| Chase / third-person camera | No | New `ChaseCameraController` in `game/` |
| Vehicle game object | No | New `GameVehicle` in `game/` |
| Opponent AI (racing + ramming) | No | New `ai/` module: waypoint graph, steering, state machine |
| Pedestrian NPCs | No | New `GamePedestrian` + ragdoll physics |
| Ragdoll physics | No | Jolt has articulated rigid bodies; needs wiring |
| Vehicle damage model | No | Per-panel damage states, visual degradation |
| Race rules / game mode | No | New `gamemode/` or `game/` subsystem: timer, checkpoints, objectives, scoring |
| Power-up system | No | Pickup `GameObject` + effect registry |
| Race HUD | Partial | `HUDScreen` exists; needs race-specific widgets (timer, damage bar, opponent list, ped counter) |
| Input remapping / gamepad | No | Only keyboard/mouse today; driving needs analog axis support |
| Open-world / track level design | No | Data work: race levels built in editor with checkpoints placed as GameObjects |
| Collision response feedback | Partial | Particles exist; need crash sounds, sparks, debris, impact force thresholds |
| Road / track surface on terrain | No | New `track/` module: spline-defined road mesh generator + tile overlay system |
| Special effects (explosions, electricity, fire) | Partial | Particle system exists; needs a higher-level `vfx/` module for composited multi-element effects |
| Project rename | No | Repo, CMake targets, and README still named `claudeengine` — must be updated to Wreckoning |
| Play-in-editor | No | In-process Play/Stop mode in the editor: auto-save → init game systems → restore on Stop |
| Resource editor framework | No | Resource browser + per-type inspector panels; all descriptors serialisable to/from YAML |
| Vehicle editor panel | No | Body/wheel mesh import, interactive wheel placement in 3D, physics parameter sliders, damage mesh assignment |
| VFX editor panel | No | Per-effect parameter editing with live in-editor preview |
| Pedestrian / pickup / road editor panels | No | Inspector panels for each resource type introduced per milestone |

---

## Detailed System Designs

### 1. Vehicle Physics (`physics/`)

**What Jolt provides:** `VehicleConstraint` with configurable wheel positions, suspension travel, spring stiffness, anti-roll bars, differential, engine torque curve, and braking.

**What to add:**

```
physics/
  VehicleDesc.h          — POD: wheel positions, suspension, engine, mass
  PhysicsVehicle.h/.cpp  — wraps VehicleConstraint; exposes Throttle/Brake/Steer/Handbrake
```

`PhysicsVehicle` must stay Jolt-free in its header (same rule as `PhysicsBody`). The game layer calls `Throttle(float)`, `Steer(float)`, `Brake(float)` each frame.

**Key parameters to tune:**
- Suspension rest length and stiffness (determines ride height and bounciness)
- Engine max torque and torque curve (acceleration feel)
- Max steer angle (turning radius)
- Front/rear brake distribution (drift vs. understeer)

---

### 2. Chase Camera (`game/`)

A spring-arm camera that follows the vehicle with lag — the classic third-person driving camera.

```
game/
  ChaseCameraController.h/.cpp
```

**Behaviour:**
- Arm length ~8–12 m behind and ~3 m above the vehicle pivot
- Spring interpolation on position (smooths bumps) and a tighter spring on look-at (snappier)
- Collision check via `PhysicsSystem::Raycast` so the camera does not clip through walls
- Optional look-ahead: shifts target point slightly in the direction of travel

**Inputs:** reads vehicle world transform from `GameVehicle` each frame; no direct input handling.

---

### 3. Vehicle Game Object (`game/`)

```
game/
  GameVehicle.h/.cpp      — scene object: owns PhysicsVehicle + body mesh + wheel meshes
  VehicleDesc.h           — game-level desc (mesh paths, physics desc, damage thresholds)
```

`GameVehicle` is a `GameObject` subclass. On each update it:
1. Reads wheel transforms from `PhysicsVehicle` and applies them to wheel `GameMesh` children.
2. Applies input (throttle / steer / brake) from the active controller (player or AI).
3. Updates damage state when collision impulse exceeds threshold.

**Player vs. AI distinction:** controlled by an `IVehicleController` interface (similar to `ICameraController`). The player has a `PlayerVehicleController` reading keyboard/gamepad; AI vehicles have an `AIVehicleController`.

---

### 4. AI System (`ai/` — new module)

This is the most substantial new module. Carmageddon AI does not need to be sophisticated — it needs to be aggressive and fun to fight.

**Dependency:** `ai/ → game/ → physics/`

#### 4a. Waypoint Graph

```
ai/
  WaypointGraph.h/.cpp    — directed graph of 3D points loaded from map YAML
  WaypointFollower.h/.cpp — finds next waypoint, steers toward it
```

Waypoints are placed in the editor and serialised into `.map.yaml`. Each node stores position and optional links to neighbours. This is simpler than a full navmesh and sufficient for Carmageddon-style track navigation.

#### 4b. AI Vehicle Controller

```
ai/
  AIVehicleController.h/.cpp
```

A simple state machine:

| State | Behaviour |
|---|---|
| `Racing` | Follow waypoint graph, avoid walls via side raycasts |
| `Ramming` | Steer directly toward player when within range and line-of-sight |
| `Recovering` | Vehicle is stuck or flipped; apply reverse/handbrake to recover |
| `Wrecked` | Vehicle health at zero; stop all control, trigger wreck VFX/SFX |

Transitions: `Racing → Ramming` when player is within ~30 m; `Ramming → Racing` after a few seconds without a hit; `any → Recovering` when speed is near zero for > 2 s.

---

### 5. Pedestrian System (`game/`)

Pedestrians are simple NPCs: they wander waypoints, flee when a car approaches, and switch to ragdoll on contact.

```
game/
  GamePedestrian.h/.cpp   — scene object: capsule physics body, animated mesh, state machine
  PedestrianDesc.h        — mesh path, walk speed, flee radius
```

**States:** `Idle → Walking → Fleeing → Ragdoll → Dead`

**Ragdoll:** on high-impulse collision with a vehicle, swap the capsule body for a `JoltRagdoll` (a chain of rigid bodies matching bone transforms). Jolt has native ragdoll support via `Ragdoll` class — needs wiring in `physics/`.

```
physics/
  PhysicsRagdoll.h/.cpp   — wraps JPH::Ragdoll; exposes Activate(boneTransforms)
  RagdollDesc.h           — per-bone shape + joint limits
```

Pedestrian splat counter increments when a pedestrian enters `Dead` state; this feeds the race objective system.

---

### 6. Vehicle Damage Model (`game/`)

Progressive damage tracked per zone (front, rear, left, right, roof).

```
game/
  VehicleDamage.h/.cpp    — per-zone HP, maps impulse → damage, notifies listeners
```

**Visual degradation:** swap mesh at damage thresholds (e.g., 0 % → pristine, 30 % → dented, 60 % → crumpled, 100 % → wrecked). Mesh variants are referenced in `VehicleDesc`.

**Gameplay effects at damage thresholds:**
- 40 % front damage → reduced steering
- 60 % engine damage → reduced max speed
- 80 % any → smoke particles on body
- 100 % → car disabled, wreck explosion

---

### 7. Race Rules / Game Mode (`game/`)

```
game/
  RaceMode.h/.cpp         — owns timer, checkpoint list, objectives, scoring
  Checkpoint.h/.cpp       — trigger volume (AABB or sphere); fires event on player entry
  RaceObjective.h         — enum: Laps | WreckAll | SplatAll
```

**Win conditions (configurable per map):**
- `Laps`: complete N laps through all checkpoints in order.
- `WreckAll`: reduce all opponent vehicles to 0 HP.
- `SplatAll`: run over every pedestrian on the map.
- (Typically the first condition met wins — matching original Carmageddon.)

**Time system:** a global countdown timer. Time bonuses are awarded for:
- Wrecking an opponent (+30 s)
- Splatting a pedestrian (+5 s)
- Passing a checkpoint (+15 s)

Timer reaching zero = game over (unless a bonus buys more time).

**Serialisation:** win conditions and checkpoint positions stored in `.map.yaml` under a `race:` key.

---

### 8. Power-Up System (`game/`)

```
game/
  GamePickup.h/.cpp       — scene object: trigger sphere, mesh, respawn timer
  IPickupEffect.h         — interface: Apply(GameVehicle&)
  PickupRegistry.h/.cpp   — maps pickup type string → IPickupEffect factory
```

**Carmageddon-inspired effects to implement first:**
- Freeze opponents (zero throttle for N seconds)
- Turbo boost (multiply engine torque)
- Invulnerability (ignore damage)
- Pinball (huge collision restitution)
- Repair (restore vehicle HP)

Power-ups are placed in the editor as `GamePickup` objects with a `type:` field in YAML.

---

### 9. Race HUD

Extend `HUDScreen` (or create `RaceHUDScreen`) with:

- **Speedometer** — analog dial sprite + digital readout
- **Damage bar** — colour-coded per-zone, or a single overall bar
- **Timer** — countdown, flashes red when < 30 s
- **Opponent list** — name + status (Racing / Wrecked)
- **Splat / wreck counters**
- **Mini-map** — top-down 2D projection of waypoints + vehicle positions (sprites)

---

### 10. Input — Gamepad / Analog Axes

Currently only keyboard and mouse are abstracted in `abstract/`. Driving needs:
- Left stick X → steer
- Right trigger → throttle
- Left trigger → brake
- A / cross → handbrake

**What to add:**

```
abstract/
  IGamepad.h              — analog axes + digital buttons, abstract
gldevices/
  GLFWGamepad.h/.cpp      — GLFW gamepad polling implementation
```

GLFW 3.3+ has `glfwGetGamepadState` — already available as a dependency.

---

### 11. Road / Track System (`track/` — new module)

Race environments need defined road surfaces on top of terrain: paved circuits, dirt paths, bridge sections. Two complementary features are needed.

**Dependency:** `track/ → renderer/ → terrain/`

#### 11a. Spline Road Generator

```
track/
  RoadSpline.h/.cpp       — cubic spline defined by control points; serialised to map YAML
  RoadMeshBuilder.h/.cpp  — generates a ribbon mesh from a RoadSpline, conforming to terrain height
  GameRoad.h/.cpp         — scene object: owns RoadSpline + generated Mesh + PhysicsBody (static)
```

A `GameRoad` is placed in the editor by dragging control points. `RoadMeshBuilder` samples the spline at configurable resolution, projects each sample point onto the terrain heightmap, and extrudes a two-sided ribbon with UV coordinates suitable for a tiling road material (dashes, lane markings, etc.).

**Key parameters in YAML:**
- `width` — road width in metres
- `resolution` — samples per metre (quality vs. triangle count trade-off)
- `material` — road material path (asphalt, dirt, gravel…)
- `control_points` — list of 3D world positions

Physics: the generated mesh is fed to `PhysicsSystem` as a static triangle mesh body, so the car rides on the road surface naturally.

#### 11b. Terrain Tile Overlay

For placing arbitrary surface patches (concrete pads, oil slicks, mud pits, ramps):

```
track/
  TerrainTile.h/.cpp      — scene object: flat quad at terrain height, own material + physics surface props
  TileDesc.h              — size, material, physics friction override
```

`TerrainTile` is the simplest possible thing: a rectangle placed in the editor, rendered with a chosen material, and registered with the physics system to override the surface friction/restitution under the tile. This enables oil slicks (low friction), mud (high rolling resistance), and metal ramps (high restitution) without modifying the underlying terrain.

---

### 12. Special Effects (`vfx/` — new module)

The particle system handles raw emitters, but composited effects (explosion = shockwave + flash + debris + smoke trail) need a coordination layer.

**Dependency:** `vfx/ → game/ → renderer/ → particles/ → audio/`

```
vfx/
  VFXSystem.h/.cpp        — singleton; spawns, updates, and expires effect instances
  IVFXEffect.h            — interface: Play(worldPos, direction); IsFinished()
  VFXExplosion.h/.cpp     — fireball burst + shockwave impulse + point light flash + screen shake
  VFXElectricity.h/.cpp   — arc line renderer + particle sparks; connects two 3D points
  VFXFire.h/.cpp          — looping fire particle system + optional heat-distortion post-process flag
  VFXScrape.h/.cpp        — continuous sparks + metal screech sound; driven by contact normal + speed
  ScreenShake.h/.cpp      — applies translational/rotational offset to the active camera for N frames
```

#### Effect details

**Explosion:**
1. Particle burst (fire + smoke) from `ParticleSystem`
2. Point light flash: spawn a `GameLight` (omni, high intensity, short lifetime ~0.15 s)
3. Physics shockwave: `PhysicsSystem::SphereOverlap` in radius → apply outward impulse to all bodies within range
4. Screen shake: magnitude proportional to inverse distance from camera

**Electricity:**
- Rendered as a segmented line with random mid-point displacement each frame (jagged arc)
- Implemented via `WireframeRenderer::PushSegment` chains or a dedicated line-strip shader
- Particle sparks at the endpoint
- Looping crackle sound via `SoundEffectComponent`

**Scrape (continuous):**
- Triggered by `VehicleDamage` when a panel contacts geometry at speed
- Driven each frame with contact point, surface normal, and relative speed
- Emits directional sparks; intensity scales with speed

**Screen shake:**
`ScreenShake` is not a VFX effect per se — it is a signal sent to the active `ICameraController`. `ChaseCameraController` applies it as a decaying sinusoidal offset on its output transform.

---

### 13. Play-in-Editor (`editor/`)

The goal is to click a Play button in the editor, drive the current map immediately, and click Stop to return to editing — with no manual save, no process switch, and no second window.

#### Core requirement: zero resource reload on Play

**Entering Play must not reload, re-parse, or re-upload any resource that is already in memory.** The editor's scene is the game's scene. Meshes, textures, materials, and terrain geometry are already on the GPU and in CPU memory — duplicating or reloading them would waste VRAM, stall the GPU pipeline, and add perceptible latency to what should feel like an instant mode switch.

This means:

- **Terrain (`GameTerrain`) is never torn down or reloaded** across a Play/Stop cycle. Its heightmap and GPU buffers stay alive. Physics registers a body against the already-loaded heightmap data.
- **Static mesh physics bodies** are created from the in-memory `RenderableMesh` geometry, not by re-reading OBJ/FBX files from disk.
- **`MeshTemplate` cache** (`GetOrLoad()`) already deduplicates by path. The Stop reload (see below) re-parses the YAML structure but hits the cache for every asset — no GPU upload occurs.
- **Sounds and particle descriptors** loaded at editor startup remain loaded; `VFXSystem` and `SoundManager` do not flush their caches on mode transitions.

The only new resource loads allowed on Enter Play are for objects that do not already exist in the editor scene (e.g., the `GameVehicle` mesh and its wheels, if not currently placed in the map). These go through the normal `MeshTemplate::GetOrLoad()` cache and are a one-time cost on first play.

#### Snapshot strategy

The editor auto-saves the map to disk **before** entering Play, but this save is a restore point only — it is not used on the Enter path at all. On Stop, `MapLoader::Load()` re-parses the YAML to restore object transforms and properties changed during play (positions knocked around by physics, damage states, etc.), while all GPU resources are served from cache.

**Consequence:** any unsaved edits made before clicking Play are preserved (they are written to disk at the snapshot moment). Changes made during Play are discarded on Stop — the disk file is the ground truth for scene structure, not for resources.

#### Mode lifecycle

```
Editor mode  ──Play──►  Play mode  ──Stop──►  Editor mode
```

**Entering Play:**
1. Auto-save current map to the active `.map.yaml` path (snapshot — disk write only, no scene rebuild).
2. Deactivate all editor tools (selection, transform gizmo, material editor, etc.).
3. Hide editor ImGui panels; show game HUD.
4. Register physics bodies for all static scene objects from in-memory geometry (terrain heightmap, mesh triangles already in RAM).
5. Spawn `GameVehicle` at the `GamePlayerStart` position; load vehicle mesh via `MeshTemplate::GetOrLoad()` (cache hit if vehicle was already in the scene).
6. Activate `PlayerVehicleController` and `ChaseCameraController` targeting the vehicle.
7. If a `race:` key is present in the map, activate `RaceMode`.
8. Start game ticks: physics step, AI, VFX, audio.

**Exiting Play (Stop):**
1. Tear down `GameVehicle` and all game-spawned objects (release their physics bodies and renderables).
2. Remove all play-time physics bodies from `PhysicsSystem` (static bodies for scene meshes, dynamic bodies for vehicle).
3. Re-parse map YAML via `MapLoader::Load()` to restore transforms/states — all assets served from `MeshTemplate` and texture caches, no GPU uploads.
4. **Do not destroy or recreate `GameTerrain`** — patch its transform from the reloaded map data only if it changed.
5. Reactivate editor tools and ImGui panels.
6. Restore `FPSCameraController`.

#### New code

```
editor/
  PlayModeManager.h/.cpp  — orchestrates enter/exit; owns in-play GameVehicle ptr and physics body handles
```

`PlayModeManager` is not a Singleton — it is owned by `EditorApp`, which already owns `GameSystem` and the editor tool stack.

**Play/Stop button:** added to the existing editor toolbar (ImGui top bar). Keyboard shortcut: `F5` to play, `Escape` or `F5` again to stop.

#### What is *not* in scope

- Saving mid-play (the map on disk is the checkpoint; no incremental saves during play).
- Multiple play sessions without a Stop in between.
- Live editing while in Play mode (editor tools stay deactivated to avoid physics/render state conflicts).

#### Edge cases

| Situation | Handling |
|---|---|
| Map has no `GamePlayerStart` | Show editor warning toast; block entering Play |
| Auto-save fails (disk full, read-only) | Block entering Play; show error — do not enter an unrestorable state |
| Game crashes during Play | `PlayModeManager` destructor tears down game systems; editor stays alive |
| User modifies map file externally during Play | Ignored; the external change is picked up on the next Stop + reload cycle |
| Vehicle mesh not in `MeshTemplate` cache | Loaded on first Play, cached for subsequent plays; no stall on second press |

---

### 14. Resource Editor Framework (`editor/`)

All game resources are descriptor files (YAML). The editor must be able to open, edit, and save any of them with a dedicated panel. This section defines the framework and each concrete panel.

#### 14a. Framework

**Resource browser:**

```
editor/
  ResourceBrowser.h/.cpp   — file tree scoped to data/; filters by extension; double-click opens panel
```

`ResourceBrowser` is an ImGui panel docked to the editor UI. It scans `data/` recursively and lists files by type (`.vehicle.yaml`, `.vfx.yaml`, `.ped.yaml`, etc.). Double-clicking a file opens the matching inspector panel.

**Inspector panel registry:**

```
editor/
  IResourcePanel.h         — interface: Draw(path); IsDirty(); Save()
  ResourcePanelRegistry.h/.cpp — maps file extension suffix → IResourcePanel factory
```

Each resource type registers its panel factory with `ResourcePanelRegistry` at startup. When the browser opens a file, the registry instantiates the right panel. Multiple panels can be open simultaneously (tabbed). Unsaved changes are tracked per panel (`IsDirty()`); the editor prompts on close if dirty.

**Serialisation contract:**

Every descriptor struct must implement:
```cpp
static MyDesc LoadFromYaml(const YAML::Node&);
void SaveToYaml(YAML::Emitter&) const;
```

This is enforced by convention (no interface, to avoid vtable overhead on POD structs). A `YamlSerialiser` utility in `core/` provides helpers for common types (`Vec3`, `Quaternion`, colour, file path).

---

#### 14b. Vehicle Editor Panel

The richest editor panel. Covers the full vehicle authoring workflow across milestones.

**Milestone 1 — Structure:**

```
editor/
  VehiclePanel.h/.cpp      — IResourcePanel for .vehicle.yaml
```

Layout:
- **Body mesh** — file picker (opens native dialog via nfd-extended); on selection, loads mesh into a live preview viewport using `PreviewRenderer` (already exists in `renderer/`).
- **Wheels** — list of up to 4 wheels, each with:
  - Mesh file picker
  - 3D position handle: the wheel is rendered in the preview viewport; the user drags a gizmo (reuse `ImGuizmo`) to position it relative to the vehicle body.
  - Side (Front/Rear, Left/Right) selector.
- **Physics parameters** — sliders for: mass (kg), suspension rest length, suspension stiffness, engine max torque, max steer angle, front/rear brake ratio.
- **Save / Revert** buttons; dirty indicator in panel title.

All changes are reflected live in the preview viewport (wheel repositions immediately as the gizmo is dragged).

**Milestone 2 — Damage meshes:**

Extend the panel with a **Damage** tab:
- Per damage zone (Front, Rear, Left, Right, Roof): a list of mesh variants at configurable HP thresholds (e.g., 30 %, 60 %, 100 %).
- Each row: threshold slider + mesh file picker + inline preview thumbnail (rendered by `PreviewRenderer`).
- **Gameplay effects** sub-section: checkboxes/sliders for speed reduction, steering reduction at each threshold.

---

#### 14c. VFX Editor Panel

```
editor/
  VFXPanel.h/.cpp          — IResourcePanel for .vfx.yaml
```

Each VFX effect type (explosion, electricity, fire, scrape) has its own parameter set, but shares a common layout:
- **Effect type** selector (enum dropdown) — switches the visible parameter block.
- **Parameters** — numeric fields and colour pickers matching the chosen `IVFXEffect` subclass (e.g., explosion: shockwave radius, light intensity, shake magnitude; electricity: arc segments, spark rate, crackle volume).
- **Preview** button — plays the effect once in a small embedded viewport (a `PreviewRenderer` scene with a neutral backdrop). Stop button cancels a looping effect.
- **Sound** — optional sound file picker for the effect's audio.

---

#### 14d. Pedestrian Editor Panel

```
editor/
  PedestrianPanel.h/.cpp   — IResourcePanel for .ped.yaml
```

- Mesh file picker + preview.
- Walk speed, flee radius sliders.
- Ragdoll: list of bones with per-bone shape type (capsule/sphere) and dimensions, displayed as overlaid wireframes on the mesh preview.

---

#### 14e. Road Editor (in-scene, not a file panel)

Road splines (`GameRoad`) are scene objects, not standalone resource files — they are embedded in `.map.yaml`. Their editing is therefore handled by a **dedicated editor tool** (not a resource panel):

```
editor/tools/
  RoadTool.h/.cpp          — tool mode: add/move/delete spline control points; set road width and material
```

`RoadTool` is activated from the editor toolbar alongside the existing selection and transform tools. Control points are rendered as draggable spheres in the 3D viewport; the road mesh regenerates live as points are moved. The material picker reuses the existing material editor.

---

#### 14f. Pickup / Race Config Editors

```
editor/
  PickupPanel.h/.cpp       — IResourcePanel for .pickup.yaml: effect type, mesh, respawn time
  RaceConfigPanel.h/.cpp   — IResourcePanel for .race.yaml: objectives, time bonuses, checkpoint order
```

These are simpler panels — mostly form fields with dropdowns. `RaceConfigPanel` lists the checkpoints present in the current map (queried from `GameSystem`) and allows reordering them.

---

#### Editor milestone placement

| Panel | Milestone |
|---|---|
| Resource browser + `IResourcePanel` framework | Milestone 1 (Play-in-Editor) |
| Vehicle panel — structure (body, wheels, physics) | Milestone 2 (Driveable Car) |
| Road tool (in-scene) | Milestone 2 (Driveable Car) |
| VFX panel | Milestone 3 (Combat & Destruction) |
| Vehicle panel — damage tab | Milestone 3 (Combat & Destruction) |
| Pedestrian panel | Milestone 5 (Pedestrians) |
| Pickup panel | Milestone 6 (Race Rules) |
| Race config panel | Milestone 6 (Race Rules) |

---

### 15. Project Rename

The repository, CMake targets, and all user-facing strings are currently named `claudeengine` / `ClaudeEngine`. These must be updated to **Wreckoning** before the project is publicly shared.

**Scope of changes:**
- `CMakeLists.txt` — project name, `claude_engine` → `wreckoning`, `claude_editor` → `wreckoning_editor`
- `README.md` — title, binary names, all references
- `src/` — any string literals or log tags containing `ClaudeEngine`
- `CLAUDE.md` files — references to the engine name (keep CLAUDE.md filename; it is a Claude Code convention)
- `data/` — any config files referencing the binary name

This is a pure housekeeping issue with no architectural risk. It should be done as a single dedicated PR before Milestone 1 work starts to avoid noisy diffs.

---

## Phased Roadmap

### Milestone 0 — Project Rename (prerequisite)

Goal: the repo, binaries, and docs reflect the Wreckoning name before any feature work.

1. Rename CMake project and targets (`claude_engine` → `wreckoning`, `claude_editor` → `wreckoning_editor`)
2. Update `README.md` title, binary names, and all references
3. Update string literals and log tags in source
4. Single PR, no functional changes

**Deliverable:** clean repo identity before feature branches start.

---

### Milestone 1 — Play-in-Editor (do this first)

Goal: launch the current map from inside the editor with F5, walk around it with the existing FPS camera, and return to editing with Stop. No vehicle yet — that comes in M2 and will plug in automatically.

This milestone is buildable today against the existing codebase and unlocks fast iteration for every subsequent milestone.

1. `PlayModeManager` — F5/Stop toolbar button; auto-save on enter; re-parse YAML + cache-hit restore on Stop; deactivate/reactivate editor tools and ImGui panels; enforce the editor-tools/game-systems mutual exclusion invariant.
2. HUD show/hide on mode transition — hide ImGui panels in play, show `HUDScreen` overlay.
3. Resource browser + `IResourcePanel` framework — file tree scoped to `data/`, extension-based panel dispatch, dirty tracking; `YamlSerialiser` helpers added to `core/`.
4. `GamePlayerStart` validation — block entering Play with a warning toast if no player start is in the scene.

*Note: in this milestone, Play mode uses the existing `FPSCameraController`. When M2 adds `GameVehicle` and `ChaseCameraController`, `PlayModeManager` switches to them automatically — no changes to M1 code required.*

**Deliverable:** F5 enters a live walkable version of the current map; Stop restores the editor exactly as it was. The edit → test loop no longer requires leaving the editor.

---

### Milestone 2 — Driveable Car

Goal: player can drive a car around a real road surface, launched from the editor with F5.

1. `PhysicsVehicle` — Jolt `VehicleConstraint` wrapper; `VehicleDesc` POD for wheel layout, suspension, engine.
2. `GameVehicle` — scene object; owns `PhysicsVehicle`; drives body + wheel mesh transforms from physics each frame.
3. `PlayerVehicleController` — keyboard input (WASD / arrows) → throttle / steer / brake / handbrake.
4. `ChaseCameraController` — spring-arm third-person camera with raycast wall-clip.
5. `RoadSpline` + `RoadMeshBuilder` + `GameRoad` — spline road mesh conforming to terrain heightmap; static physics body.
6. `VehiclePanel` (resource editor) — body + wheel mesh import, interactive wheel gizmo positioning in `PreviewRenderer`, physics parameter sliders; serialises to `.vehicle.yaml`.
7. `RoadTool` (in-scene editor tool) — drag control points, live road mesh regeneration.
8. Test map — terrain, a closed road loop, a few static obstacles, one `GameVehicle` player start.

**Deliverable:** a car you can drive, steer, and crash on a real road — launched from the editor with F5 in under a second.

---

### Milestone 3 — Combat & Destruction

Goal: cars collide with real consequences.

1. `VehicleDamage` — per-zone HP, impulse listener.
2. Visual damage mesh swaps; **Vehicle panel — Damage tab** (per-zone meshes at configurable HP thresholds).
3. Crash sound effects (`SoundEffectComponent` on impact).
4. `VFXScrape` — sparks + screech on panel contact.
5. `VFXExplosion` — fireball, light flash, shockwave, screen shake; **VFX panel** for editing effect parameters.
6. `VFXFire` / smoke particles at high damage.
7. `TerrainTile` — oil slick and ramp tiles on test track.
8. Wreck state: explosion + disabled vehicle.

**Deliverable:** collisions feel impactful; cars degrade and eventually blow up.

---

### Milestone 4 — Opponents

Goal: AI cars race and ram the player.

1. Waypoint graph — placed in editor, serialised to YAML.
2. `AIVehicleController` — state machine (Racing / Ramming / Recovering / Wrecked).
3. `VehicleDamage` applied to AI cars too.
4. Basic opponent HUD list.

**Deliverable:** fight 2–3 AI opponents on a test track.

---

### Milestone 5 — Pedestrians

Goal: pedestrians populate the map and can be splatted.

1. `GamePedestrian` — animated mesh, capsule physics, flee AI; **Pedestrian panel** for mesh + ragdoll setup.
2. `PhysicsRagdoll` — Jolt ragdoll activated on high-impulse hit.
3. Blood/gore particles on splat.
4. Splat counter in HUD.

**Deliverable:** pedestrians walk, flee, and die messily.

---

### Milestone 6 — Race Rules

Goal: a complete game loop.

1. `Checkpoint` trigger volumes — placed in editor.
2. `RaceMode` — timer, objectives, win/lose detection; **Race config panel**.
3. Power-up `GamePickup` system with 3–4 effects; **Pickup panel**.
4. Full race HUD (timer, damage, opponents, splats).
5. Results screen on win/lose.

**Deliverable:** a complete Wreckoning game loop from start to finish.

---

### Milestone 7 — Polish

Goal: it feels like a real game.

1. Gamepad support (`GLFWGamepad`).
2. Main menu and map select screen.
3. Vehicle select screen (2–3 cars with different stats).
4. Multiple race levels with road splines and tile overlays built in editor.
5. `VFXElectricity` power-up visual.
6. Sound mix: engine pitch scales with RPM, tyre screech on slide, ambient crowd sounds.

---

## Risk Assessment

| Risk | Severity | Mitigation |
|---|---|---|
| Vehicle physics feel | High | Jolt VehicleConstraint is well-tested; budget time for parameter tuning (suspension, torque, mass) |
| AI navigation quality | High | Keep it simple — waypoint graph + raycasts, no navmesh. Fun > realistic. |
| Ragdoll stability | Medium | Jolt ragdolls can be jittery at low velocity; clamp to `Dead` state after settling |
| Mesh deformation | Low | Not implementing per-vertex deformation — mesh swap at thresholds is sufficient and cheaper |
| Gamepad latency | Low | GLFW polling is synchronous; no extra latency expected |
| Level design effort | Medium | Editor is already functional; waypoints and checkpoints need editor tool support |
| Road mesh terrain conformance | Medium | Spline samples must project onto terrain heightmap accurately; edge cases at terrain boundaries |
| Electricity arc rendering | Medium | No line-strip shader exists yet; reuse `WireframeRenderer` or add a dedicated pass |
| Play mode state conflict | Low | Physics and editor tools must not be active simultaneously; `PlayModeManager` is the single owner of that invariant |

---

## Architecture Notes

- **No new top-level module for vehicles** — `GameVehicle` lives in `game/`, `PhysicsVehicle` lives in `physics/`. This keeps the dependency graph clean.
- **`ai/` is a new module** depending on `game/` and `physics/`. It must not be included by `renderer/` or `physics/`.
- **`RaceMode` is not a `GameObject`** — it is a plain class owned by the application layer (`app/`), similar to how `GameSystem` is owned today.
- **Checkpoints are `GameObject` subclasses** — they appear in `.map.yaml`, are selectable in the editor, and receive transform updates like any other scene object.
- **`track/` is a new module** depending on `renderer/` and `terrain/`. `RoadMeshBuilder` reads terrain height from `GameTerrain`; it must not depend on `game/` directly — pass the height query as a `std::function` to keep the dependency clean.
- **`vfx/` is a new module** depending on `game/`, `particles/`, `audio/`, and `renderer/`. `VFXSystem` is a Singleton, like `SoundManager`.
- **`ScreenShake`** is part of `vfx/` but communicates with the camera via an event or a direct call on `ICameraController`; it must not hardcode `ChaseCameraController`.
- **`PlayModeManager`** is owned by `EditorApp`, not by `GameSystem`. It is the single point that enforces the invariant that editor tools and game systems are never both active. All mode-transition logic lives there — no mode flags scattered across other classes.
- All new modules must have a `CMakeLists.txt` and an entry in `src/CMakeLists.txt`.

---

## Starting Point: Issues to Open

### Milestone 0 — Rename

1. **chore: rename project to Wreckoning** — update CMake targets, README, log tags, and YAML config references in one PR before feature work starts.

### Milestone 1 — Play-in-Editor

2. **`editor`: add `PlayModeManager`** — F5/Stop toolbar button; auto-save on enter; YAML re-parse + cache-hit restore on Stop; deactivate/reactivate editor tools and ImGui panels; `GamePlayerStart` validation with warning toast.
3. **`editor`: add resource browser and `IResourcePanel` framework** — file tree scoped to `data/`, extension-based panel dispatch, dirty tracking; `YamlSerialiser` helpers added to `core/`.

### Milestone 2 — Driveable Car

4. **`physics`: add `PhysicsVehicle` using Jolt `VehicleConstraint`** — expose `Throttle`, `Steer`, `Brake`, `Handbrake`; `VehicleDesc` POD for wheel layout and engine params.
5. **`game`: add `GameVehicle` scene object** — owns `PhysicsVehicle`; drives body + wheel mesh transforms from physics output each frame.
6. **`game`: add `PlayerVehicleController`** — reads keyboard input (WASD / arrows) and maps to `GameVehicle` control inputs.
7. **`game`: add `ChaseCameraController`** — spring-arm camera tracking a target `GameVehicle`; raycast-based wall clipping.
8. **`track`: add `RoadSpline`, `RoadMeshBuilder`, `GameRoad`** — spline road mesh conforming to terrain heightmap; static physics body.
9. **`editor`: add `VehiclePanel`** — body mesh import with `PreviewRenderer` viewport, per-wheel mesh + 3D gizmo positioning, physics parameter sliders; serialises to `.vehicle.yaml`.
10. **`editor`: add `RoadTool`** — in-scene spline control point editing; live road mesh regeneration as points are dragged.
11. **data: create test race map** — terrain, a closed road loop using `GameRoad`, a few static obstacles, one `GameVehicle` player start.
