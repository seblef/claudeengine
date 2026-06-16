# Foam Texture UV Scale and Scroll Speed Configurable

**Issue**: #575
**PR**: #589
**Branch**: `feat/water-foam-tex-params-configurable-575`

## Changes

### `src/environment/WaterInfos.h`
- Added a new 8th `float4` block (offset 112–124) containing:
  - `foam_scale1 = 0.04f` — UV zoom for first foam sample
  - `foam_scale2 = 0.07f` — UV zoom for second foam sample
  - `foam_scroll_speed1 = 0.015f` — signed scroll speed for first sample
  - `foam_scroll_speed2 = -0.010f` — signed scroll speed for second sample (negative = counter-scroll)
- Updated std140 offset comment block to reflect 128 bytes / 8 float4s
- Updated `static_assert(sizeof(WaterInfos) == 112)` → `== 128`

### `data/shaders/glsl/uniforms/water_infos.glsl`
- Added `foam_tex_params` vec4 as the 8th member of `WaterInfosBlock`
- Updated header comment: 112 → 128 bytes, 7 → 8 float4s, added offset 112 entry

### `data/shaders/glsl/water/water_ps.glsl`
- Replaced hardcoded `0.04`, `0.015` with `foam_tex_params.x`, `foam_tex_params.z`
- Replaced hardcoded `0.07`, `0.010` with `foam_tex_params.y`, `foam_tex_params.w`
- Updated the header equation comment to show the parametric form
- Both sample lines now use `+ time * foam_tex_params.z/w`; the sign is carried by the default value of `foam_scroll_speed2 = -0.010` (signed speed scalar convention)

### `src/environment/EnvironmentDesc.h`
- Added `foam_scale1 = 0.04f`, `foam_scale2 = 0.07f`, `foam_scroll_speed1 = 0.015f`, `foam_scroll_speed2 = -0.010f`

### `src/game/MapLoader.cpp`
- Added YAML parsing for the four new fields after `foam_speed`

### `src/editor/MapSerializer.cpp`
- Added YAML emission for the four new fields after `foam_speed` (skipped when equal to defaults)

### `src/environment/WaterRenderer.h`
- Added `SetFoamTexParams(scale1, scale2, speed1, speed2)` setter
- Added four private members (`foam_scale1_` etc.) with correct defaults
- Added four getters (`GetFoamScale1()` etc.)

### `src/renderer/Renderer.cpp`
- Wired `wi.foam_scale1/2` and `wi.foam_scroll_speed1/2` from WaterRenderer getters in the WaterInfos upload block

### `src/editor/EnvironmentEditorPanel.cpp`
- Called `SetFoamTexParams` in `EnableWater()` alongside existing `SetFoamParams`
- Added four `SliderFloat` widgets in `RenderWaterSection()`:
  - "Foam tex scale 1" — range [0.005, 0.3]
  - "Foam tex scale 2" — range [0.005, 0.3]
  - "Foam scroll speed 1" — range [-0.1, 0.1]
  - "Foam scroll speed 2" — range [-0.1, 0.1]

### `src/app/main.cpp`
- Called `SetFoamTexParams` after `SetFoamParams` in the game-app map load path

## Decisions

**Sign convention**: The negative counter-scroll on sample 2 is encoded in the default value (`-0.010f`) rather than in a hardcoded minus sign in the shader. This collapses direction and magnitude into one signed scalar, consistent with the issue spec ("no separate direction parameter"). The shader reads `+ time * foam_tex_params.w` for both samples.

**Slider range for scroll speeds**: ±0.1 covers the default magnitudes (0.015 / 0.010) with plenty of headroom. The slider allows negative values so users can reverse scroll direction without opening the YAML.

## Skills / CLAUDE.md instructions used

- `impl-issue` skill — branch from `dev`, conventional commit, PR to `dev`
- `src/environment/CLAUDE.md` — cppcheck-suppress pattern for unused struct members
- `data/shaders/glsl/CLAUDE.md` — update equation comments in shader header

## Notes for future contributions

- `WaterInfos` is now 128 bytes (8 float4s). The next addition will require a 9th float4 at offset 128.
- The foam texture sampling blend weights (`0.6` / `0.4`) and the `2.5` contrast multiplier remain hardcoded; they could be made configurable similarly if needed.
- Pattern for exposing a new WaterRenderer parameter: add to `EnvironmentDesc` → `MapLoader` → `MapSerializer` → `WaterRenderer` (setter + private member + getter) → `Renderer` WaterInfos upload → `EnvironmentEditorPanel` (EnableWater + RenderWaterSection) → game-app `main.cpp`.
