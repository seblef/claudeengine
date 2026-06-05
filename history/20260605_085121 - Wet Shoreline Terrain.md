# Wet Shoreline Terrain (#360)

## What was done

Added a wet shoreline effect to the terrain G-buffer fragment shader (`data/shaders/glsl/terrain/terrain_ps.glsl`). Terrain fragments at or below the water surface are darkened (simulating water absorption) and given a specular boost (simulating the smooth, reflective surface of wet rock or sand). A smooth transition fades the effect out 1 metre above the waterline.

## Changes

**`data/shaders/glsl/terrain/terrain_ps.glsl`** — only file modified.

- Updated header comment for `location 2 (Specular)` to reflect that specular intensity is no longer always zero.
- Added wet shoreline block after the caustic projection block, reusing the already-declared `world_y` and `water_lvl` variables (introduced by #359).
- Changed `out_specular` to use a computed `specular_intensity` variable instead of a hardcoded `0.0`.

## Key decisions

**Reuse of existing variables**: `world_y = v_world_pos.y` and `water_lvl = water_params.a` were already declared by the caustic code (#359). No duplication, no additional VS outputs needed.

**Placement after caustics**: The wet block is placed after caustics so that the darkened albedo does not affect the caustic brightness calculation (caustics already run only when submerged).

**Hardcoded constants**: `kWetMargin`, `kWetDarken`, `kWetSpecular` are local shader constants per the issue spec. They can be promoted to `WaterInfos` parameters if editor control is needed later.

**Specular shininess unchanged**: The G channel of the specular buffer (shininess, `0.02`) is left unchanged. The wet effect only boosts the intensity (R channel). Shininess could also be tuned later.

## Constants

| Constant | Value | Meaning |
|---|---|---|
| `kWetMargin` | 1.0 m | Distance above water level where wet effect fades to zero |
| `kWetDarken` | 0.5 | Albedo multiplier at full wetness (50% darker) |
| `kWetSpecular` | 0.4 | Specular intensity (G-buffer R) at full wetness |

## Dependencies

- **#352** — promotes `water_cb_` to a global UBO binding (binding 9) so `water_params.a` (water level) is accessible in the terrain geometry pass.
- **#359** — caustic projection: already included `water_infos.glsl` and extracted `world_y` / `water_lvl` into the `main()` scope.

## Skills / instructions used

- `impl-issue` skill
- `CLAUDE.md` → conventional commits, history file requirement, branch naming
- `data/shaders/glsl/CLAUDE.md` → custom `#include`, documentation in comments, equations

## Notes for future work

- `kWetDarken`, `kWetMargin`, `kWetSpecular` are candidates for `WaterInfos` uniforms if the designer wants runtime control.
- A subtle normal perturbation (slight flattening / glossing of the blended normal) could complement the specular boost for an even more convincing wet look.
