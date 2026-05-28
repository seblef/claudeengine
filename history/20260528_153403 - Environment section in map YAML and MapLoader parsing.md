# Environment section in map YAML and MapLoader parsing

**Issue:** #326
**Branch:** `feat/environment-map-yaml-326`

## Goal

Wire the existing `EnvironmentDesc` struct (from issue #319) into the full
map loading / serialisation / app-integration pipeline so that each environment
subsystem can be enabled or disabled per map via the YAML `environment:` key.

## Changes

### `src/game/MapLoader.cpp`

`ParseEnvironmentDesc()` already existed from issue #319 but was missing one
invariant: the `wind_direction` vector was stored raw from YAML without being
normalised. Added a `.Normalized()` call after parsing so shaders and `WindSystem`
always receive a unit XZ vector regardless of what the level designer wrote.

### `src/editor/EditorScene.h`

Added an `environment::EnvironmentDesc environment_desc_` member plus a
`GetEnvironmentDesc() const` accessor and a `SetEnvironmentDesc(const …&)` mutator.
This lets the editor round-trip the environment section through load → save without
needing to expose every parameter in the UI yet.

### `src/editor/MapSerializer.cpp`

Added two things:

1. A free helper `EmitEnvironment(YAML::Emitter&, const EnvironmentDesc&)` in the
   anonymous namespace. It only emits fields that differ from their `EnvironmentDesc`
   defaults, keeping the YAML readable for the common case (e.g. if only `sky_enabled`
   is `true`, only that key appears).

2. A call to `EmitEnvironment` in `Save()`, placed after the `global_light` block
   and before the `objects` sequence.

3. In `Load()`, after constructing the `EditorScene`, `SetEnvironmentDesc()` is called
   with `map_data.environment_desc` so the round-trip is complete.

### `src/app/main.cpp`

Replaced the single `if (sky_enabled)` block with a full per-subsystem init sequence:

| Flag | What happens |
|---|---|
| `sky_enabled` | `WorldTime` + `SkyRenderer` (unchanged behaviour) |
| `water_enabled` | `WaterRenderer::Build(video, water_level)` + `SetWaterRenderer()` |
| `cloud_enabled` | `CloudRenderer::Build(video)` + `SetCloudRenderer()` + `SetCloudDensity()` |
| `wind_enabled` | `WindSystem(env)` stored in `map_wind_system` + `SetWindSystem()` |
| `trees_enabled` | `SetTreeEnabled(true)` forwarded to `Renderer` |

The main loop was also updated:
- The `prev_elapsed` / `frame_dt` block now fires when *either* `map_world_time`
  or `map_wind_system` is active (previously only when both world_time and global_light
  were present).
- `SetWaterSkyWorldTime()` is synchronised with `SetSkyWorldTime()` each frame.
- `map_wind_system->Update(frame_dt)` is called each frame.

Shutdown was extended to call `Reset()` + `Shutdown()` for `WaterRenderer` and
`CloudRenderer` singletons when they are instanced.

## Decisions and rationale

**`wind_direction` normalisation in loader, not in `EnvironmentDesc`**  
The struct is intentionally plain data with no behaviour. Normalising at the parse
boundary keeps the rule "what you store is valid" consistent with how
`ParseGlobalLightDesc` already normalises `direction`.

**Emit only non-default fields**  
The issue explicitly prefers this. A newly-created map with no environment settings
will emit an empty `environment: {}` block (or nothing meaningful), keeping YAML
diff noise low during development.

**`trees_enabled` maps to `SetTreeEnabled()` only**  
`TreeRenderer::Build()` requires actual `TreeLayer` objects which come from the
`objects:` list (parsed as `type: tree_layer` — future issue). The flag only
enables/disables the draw path; the renderer handles a no-layers state gracefully.

## Output to keep in mind for the next features

- `wind_direction` is always normalised by the time `WindSystem` receives it; no
  need to normalise again in `WindSystem`.
- `WaterRenderer`, `CloudRenderer`, `WindSystem` all follow the same singleton
  pattern as `SkyRenderer`; future features should mirror that pattern.
- `EditorScene` now carries `EnvironmentDesc`; editor panels can read/write it
  via `GetEnvironmentDesc()` / `SetEnvironmentDesc()`.
- The `type: tree_layer` object type is not yet parsed in `MapLoader.cpp`; that
  is left for the tree-layer issue.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — Google C++ style, one class per file, project-relative includes
- `src/game/CLAUDE.md` — game module dependency constraints
- `src/environment/CLAUDE.md` — environment module dependency constraints
- `src/editor/CLAUDE.md` — editor is the leaf of the dependency graph
