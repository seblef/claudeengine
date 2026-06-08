# Water Env Params Game/Editor Sync & Cloud Speed

## Problem

Two unrelated visual issues were reported:

1. **Water normal maps scroll at different rates in the game vs. the editor.** Specifically, they scrolled faster in the game app.
2. **Clouds move too slowly** and should be a bit faster.

## Root Cause: Water Params Not Applied in Game Loop

`src/app/main.cpp` only passed `water_level` to `WaterRenderer::Build()`:

```cpp
new environment::WaterRenderer();
environment::WaterRenderer::Instance().Build(
    video, env.water_level, terrain_w, terrain_h);
```

All other water parameters loaded from the map's YAML into `EnvironmentDesc` were silently ignored:
- `water_color_r/g/b`
- `roughness`, `sun_intensity`, `refraction_strength`, `absorption_scale`
- `foam_height_thresh`, `foam_shoreline_depth`, `foam_steepness_thresh`, `foam_speed`
- **`normal_scale1/2`, `normal_scroll_speed1/2`** (the direct cause of the visible scroll discrepancy)

The editor (`EnvironmentEditorPanel`) correctly called all setters when applying the desc to the water renderer.

For the `demo3` map, the stored values are:
- `normal_scroll_speed1: 0.02` (vs. default `0.30` — 15× slower)
- `normal_scroll_speed2: 0.04` (vs. default `0.20` — 5× slower)

So the game ran with default speeds (fast) while the editor loaded the map values (slow).

## Fix

### `src/app/main.cpp`

After `Build()`, apply the full set of water parameters from `env`:

```cpp
environment::WaterRenderer& wr = environment::WaterRenderer::Instance();
wr.Build(video, env.water_level, terrain_w, terrain_h);
wr.SetWaterColor(env.water_color_r, env.water_color_g, env.water_color_b);
wr.SetRoughness(env.roughness);
wr.SetSunIntensity(env.sun_intensity);
wr.SetRefractionStrength(env.refraction_strength);
wr.SetAbsorptionScale(env.absorption_scale);
wr.SetFoamParams(env.foam_height_thresh, env.foam_shoreline_depth,
                 env.foam_steepness_thresh, env.foam_speed);
wr.SetNormalMapParams(env.normal_scale1, env.normal_scale2,
                      env.normal_scroll_speed1, env.normal_scroll_speed2);
```

### `data/shaders/glsl/clouds/clouds_ps.glsl`

Increased `kCloudSpeed` from `0.00003` to `0.0001` (~3.3×). At 3 m/s wind the
UV now drifts at 0.0003 UV/s, making clouds visibly but gently move.

## Decisions

- **Why 0.0001?** The user asked for "a bit faster" — 3.3× feels like a moderate
  increase. At typical wind speeds (3–10 m/s) the resulting cloud drift is
  perceptible without being distracting.
- **LOD/distance thresholds were not touched** — these are separate from the params
  above and were not reported as a problem.

## Files Changed

- `src/app/main.cpp` — apply all water parameters from `EnvironmentDesc`
- `data/shaders/glsl/clouds/clouds_ps.glsl` — bump `kCloudSpeed`

## Skills / Instructions Used

- `src/CLAUDE.md`, `src/app/CLAUDE.md` — Google C++ style, include-path conventions
- `data/shaders/glsl/CLAUDE.md` — shader naming and include rules
- `CLAUDE.md` (root) — git workflow (branch from main, cpplint, PR)

## For Next Time

- When adding a new field to `EnvironmentDesc`, verify it is applied in **both**
  `EnvironmentEditorPanel::EnableWater/Sky/Cloud/Wind` **and** `src/app/main.cpp`.
  The two code paths must stay in sync.
