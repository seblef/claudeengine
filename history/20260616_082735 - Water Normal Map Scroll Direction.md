# Water Normal Map Configurable Scroll Direction

**Issue:** #545  
**PR:** #548  
**Branch:** `feat/water-normal-map-direction-545`  
**Date:** 2026-06-16

## Summary

Added support for configuring the UV-space scroll direction of each water normal map layer. Previously both directions were hardcoded in the fragment shader (`vec2(1.0, 0.6)` and `-vec2(0.7, 1.0)`). They are now configurable parameters that flow through every layer of the system: GPU constant buffer → shader, C++ runtime, map YAML, and editor UI.

## Changes

### GPU / Shader

- `WaterInfos` (96→112 bytes, 6→7 float4s): added `normal_dir1_x/y`, `normal_dir2_x/y` in a new seventh float4.
- `water_infos.glsl`: added `dir_params` vec4 to the UBO (`dir_params.xy` = layer-1 direction, `dir_params.zw` = layer-2 direction).
- `water_ps.glsl`: replaced hardcoded `vec2(1.0, 0.6)` / `-vec2(0.7, 1.0)` with `dir_params.xy` / `dir_params.zw`.

### C++ Runtime

- `EnvironmentDesc.h`: added four `normal_dir1_x/y`, `normal_dir2_x/y` fields. Defaults are normalised forms of the old hardcoded vectors so existing maps are unaffected.
- `WaterRenderer.h`: added `SetNormalMapDirections()`, four private members, and four getters.
- `Renderer.cpp`: updated `WaterInfos` fill block to copy direction fields from `WaterRenderer`.

### Persistence

- `MapLoader.cpp`: reads `normal_dir1` and `normal_dir2` as two-element float sequences; absent keys leave defaults intact (backwards compatible).
- `MapSerializer.cpp`: emits `normal_dir1` / `normal_dir2` only when they differ from the struct default (sparse YAML).

### Editor

- `EnvironmentEditorPanel.h`: added `normal_dir1_angle_` / `normal_dir2_angle_` cached state.
- `EnvironmentEditorPanel.cpp`: added `UVDirToAngleDeg()` / `AngleDegToUVDir()` helpers; `SetContext()` initialises the cached angles; `EnableWater()` calls `SetNormalMapDirections()`; `RenderWaterSection()` exposes two 0–360° angle sliders that immediately update the renderer.

## Decisions

- **Storing normalised floats rather than an angle**: the GPU consumes a direction vector directly; storing components avoids a per-frame trigonometry call in the shader.
- **Angle-based editor UI**: a 0–360° slider is more intuitive for artists than separate X/Y components; conversion is done in the editor, not the runtime.
- **Backward compatibility**: default values are chosen to reproduce the old hardcoded directions exactly, so existing map files work without modification.
- **No SSR shader change needed**: `water_ssr_ps.glsl` uses the Gerstner macro normal (not the normal map) for its ray-march direction, so no update was required there.

## Output to keep in mind

- `WaterInfos` is now 112 bytes (7 float4s). Any future addition must continue in float4-aligned increments.
- The YAML keys for directions are `normal_dir1` and `normal_dir2` (two-element float sequences).
- `kWaterInfosFloat4s` comment in `Renderer.cpp` now reads `// 112 / 16 = 7`.

## Skills / instructions used

- `impl-issue` skill (GitHub issue implementation workflow)
- `src/CLAUDE.md`, `src/environment/CLAUDE.md`, `src/editor/CLAUDE.md`: one class per file, Google C++ style, no platform includes
- `data/CLAUDE.md`, `data/shaders/glsl/CLAUDE.md`: shader naming and include conventions
- `history/` file requirement from project-level `CLAUDE.md`
