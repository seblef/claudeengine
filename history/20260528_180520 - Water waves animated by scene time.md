# Water Waves Animated by Scene Time — Issue #342

## Summary

Water waves were static whenever the wind system was not running (wind disabled,
or editor without the Tick() fix from issue #341).  The root cause was that the
wave phase was driven by `wind_time` (only advances when `WindSystem::Update()` is
called) rather than `time` from `scene_infos` (always advances).

## Root cause

The water vertex shader computed wave phase as:

```glsl
float phase = dot(dirs[i], xz) * freqs[i] + wind_time * speeds[i];
```

`wind_time` is uploaded by `Renderer::FillWindInfos()` from
`WindSystem::GetWindTime()`.  If no `WindSystem` is registered (wind disabled in
the map or not yet ticked in the editor), `wind_time` stays 0.  With `wind_time =
0` the phase is a fixed function of vertex position only — the surface deforms
into a static shape instead of animating.

`scene_infos.time` is always advancing: the game passes `GameSystem::elapsed_time_`
and the editor passes `ImGui::GetTime()` to `Renderer::Update()`.  It is already
included in the vertex shader via `#include <uniforms/scene_infos.glsl>`.

## Fix

`data/shaders/glsl/water/water_vs.glsl`:

- Replaced `wind_time` with `time` (from `scene_infos`) in the wave phase formula.
- `wind_xz` and `wind_strength` are still used for wave direction and amplitude.
- Added a comment explaining the rationale.
- Updated the UBO binding list in the header comment.

```glsl
// Before
float phase = dot(dirs[i], xz) * freqs[i] + wind_time * speeds[i];

// After
float phase = dot(dirs[i], xz) * freqs[i] + time * speeds[i];
```

This branch also includes the editor Tick() fix from issue #341 (advancing the
wind system each frame so amplitude and direction are updated live).

## Files changed

| File | Change |
|---|---|
| `data/shaders/glsl/water/water_vs.glsl` | Wave phase uses `time` instead of `wind_time` |
| `src/editor/EnvironmentEditorPanel.cpp` | `Tick()`: advance `wind_system_` each frame (from #341) |
| `history/20260528_180520 - Water waves animated by scene time.md` | This file |

## Behaviour after fix

| Scenario | Before | After |
|---|---|---|
| Water + wind enabled | Waves animated | Waves animated (unchanged) |
| Water only (no wind) | Flat surface | Waves animated (amplitude=0) |
| Editor, water + wind | Static surface | Waves animated |
| Editor, water only | Static surface | Waves animated (amplitude=0) |

Note: without wind, `wind_strength = 0` → `base_amp = 0` → no vertical displacement.
Waves animate in phase but are invisible.  To see waves, wind must be enabled with
strength > 0.  This is physically correct: no wind = flat water.

## Notes for future contributions

- A future "calm water" feature could add a small minimum amplitude (e.g., 0.05 m)
  to simulate ambient ripples even with `wind_strength = 0`.
- `wind_time` is no longer used by the water VS; the `wind_infos` UBO is still
  included for `wind_xz` and `wind_strength`.

## Skills / CLAUDE.md guidance followed

- `impl-issue` skill
- `src/CLAUDE.md` — Google C++ style
- `data/shaders/glsl/CLAUDE.md` — `_vs`/`_ps` suffix convention, `#include` for uniforms
- `src/environment/CLAUDE.md` — environment module dependency constraints
