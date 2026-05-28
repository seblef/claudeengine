# Dynamic Sun/Moon Lighting from WorldTime

**Branch**: `feat/environment-lighting-worldtime-321`
**Issue**: #321
**Date**: 2026-05-28

---

## Summary

Wires `WorldTime` into the scene's `GlobalLight` so shadow direction, sun
colour, and ambient colour update each frame to match the visible sky disc.
Also adds a `WorldTime`-aware sky session to the game app: a `SkyRenderer` is
created and registered when `environment.sky_enabled` is set in the map YAML.

---

## Changes

### New files

| File | Description |
|------|-------------|
| `src/game/EnvironmentLighting.h` | Header-only helper: `game::UpdateGlobalLight(GlobalLight&, WorldTime&)` |

### Modified files

| File | Change |
|------|--------|
| `src/game/MapLoader.h` | Added `environment::EnvironmentDesc environment_desc` to `MapData` |
| `src/game/MapLoader.cpp` | Added `ParseEnvironmentDesc()` + wires parsing of `environment:` YAML key |
| `src/app/main.cpp` | Creates `WorldTime` + `SkyRenderer` when sky enabled; calls `UpdateGlobalLight` + `SetSkyWorldTime` each frame |

---

## Architecture decisions

### `EnvironmentLighting.h` placed in `game/`, not `environment/`

The issue suggests `environment/EnvironmentLighting.h`, but the environment
module's CLAUDE.md explicitly forbids `environment → renderer` dependencies.
`UpdateGlobalLight` needs both `renderer::GlobalLight` and
`environment::WorldTime`, so it must live in a module that already depends on
both. The `game` module already links `renderer` and `environment` as public
dependencies — it is the correct home.

### Header-only inline functions

The helper is three small branches with no static state. Header-only avoids
adding a new `.cpp` to `environment/CMakeLists.txt` and keeps the compile
footprint minimal.

### Dawn/dusk threshold: `sin(15°) ≈ 0.259`

`sun_dir.y` equals `sin(elevation)`. The issue specifies a warm-to-white
colour transition when elevation < 15°, so the blend factor `t` is
`clamp(sun_dir.y / sin(15°), 0, 1)`. At the exact horizon (y=0) the colour
is fully warm orange; at 15° it reaches near-white noon colour.

### Intensity clamped to `[0.05, 1.0]`

`sin(elevation)` at latitude 45 N peaks at `sin(45°) ≈ 0.707` — not 1.0.
The issue spec says "scales with sin(elevation), clamped to [0.05, 1.0]".
Using `sun_dir.y` directly (which is `sin(elevation)`) means noon intensity
is ~0.707, which is physically reasonable for a 45° sun and produces visible
shadows without over-exposure.

### WorldTime advancement uses `GetElapsedTime()` delta

`GameSystem::GetElapsedTime()` returns total seconds since construction. The
main loop saves `prev_elapsed` and computes `frame_dt = elapsed - prev_elapsed`
each iteration. On the very first frame `frame_dt = 0`, so the sky starts at
the initial time (06:00, dawn). This avoids adding a duplicate `std::chrono`
clock to `main.cpp`.

### `EnvironmentDesc` YAML parsing in `MapLoader`

The history file from #319 noted that YAML parsing for `EnvironmentDesc` was
deferred. Issue #321 is the natural moment to add it since `sky_enabled` and
`time_scale` are needed to decide whether to create `WorldTime` and
`SkyRenderer` in the app. All fields are optional; existing maps without an
`environment:` key get the `EnvironmentDesc` defaults (`sky_enabled = false`).

### `SkyRenderer` lifetime mirrors `TerrainRenderer`

`Renderer` does not own the `SkyRenderer`. It is created with `new` (Singleton
pattern), built, and registered via `SetSkyRenderer()` before the game loop,
then `Reset()` + `Shutdown()` after the loop (before `GameSystem::Shutdown()`
to ensure the GL context is still alive).

### Sun direction consistency

`WorldTime::GetSunDirection()` feeds both `game::UpdateGlobalLight()` (shadow
direction) and `Renderer::SetSkyWorldTime()` → `SkyRenderer::Render()` (sky
disc position). Because both consumers call `GetSunDirection()` on the same
`WorldTime` instance via the same formula, the shadow and disc are always
identical — no mismatch is possible.

---

## Map YAML extension

Maps can now include an `environment:` section:

```yaml
environment:
  sky_enabled: true
  time_scale: 300.0   # 1 s real = 5 min in-game
  water_enabled: false
  cloud_enabled: false
  wind_enabled: false
  wind_strength: 3.0
  wind_direction: [1, 0, 0]
```

All keys are optional; omitting `environment:` entirely defaults to
`sky_enabled: false` (no sky, no dynamic lighting).

---

## Output to keep in mind

- `game::UpdateGlobalLight` must be called BEFORE `game.Update()` /
  `Renderer::Update()` each frame so the new direction feeds the current frame.
- `SetSkyWorldTime` must similarly be called before `Renderer::Update()`.
- The colour table from the issue is implemented as a linear interpolation
  driven by `sun_dir.y / sin(15°)`; both endpoints match the issue spec exactly.
- Night intensity is stored as `SetIntensity(0.05f)` with colour `(0.5, 0.55, 0.7)`,
  matching the issue's `(0.5, 0.55, 0.7) × 0.05` specification.
- The editor (`EditorScene`) does not yet integrate `WorldTime` — the sky is
  static in the editor; a future editor issue could add a time-of-day slider.

---

## Skills and instructions followed

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, cpplint before commit,
  conventional commit, history file required
- `src/environment/CLAUDE.md`: `environment` must not depend on `renderer`
  (hence placing the helper in `game/`)
- `src/game/CLAUDE.md`: dependency graph, no platform headers, header-only for
  data helpers
