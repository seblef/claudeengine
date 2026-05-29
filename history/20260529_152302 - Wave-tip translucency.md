# Wave-tip translucency

**Branch:** `feat/environment-wave-tip-translucency-358`
**Issue:** #358
**Date:** 2026-05-29

## Summary

Added a subsurface-scattering approximation for wave-tip translucency in `water_ps.glsl`.  
This reproduces the characteristic turquoise/cyan-teal glow visible on real ocean wave crests where water is thin and backlit by the sun.

## Changes

### `data/shaders/glsl/water/water_ps.glsl`

Added the translucency calculation between the Cook-Torrance specular term and the foam mix:

```glsl
float back_scatter = max(dot(sun_dir, -N), 0.0);
float tip_amount   = back_scatter * steepness * sun_vis;
const vec3 kTipColor = vec3(0.10, 0.80, 0.70);
vec3 translucency    = kTipColor * tip_amount * sun_params.w * 0.15;
final_color += translucency;
```

Also added the corresponding equation block to the file's header comment.

## Design decisions

- **Variable reuse:** `steepness` (already computed for steepness-foam at line 140) and `sun_dir`, `sun_vis`, `sun_params.w` (all from the specular block) are reused directly — no new uniforms needed.
- **Macro normal for steepness:** `v_world_normal.y` (Gerstner analytical normal from the vertex shader) is used rather than the combined normal-map `N` so the effect responds to large wave shapes only. The micro normal-map detail is excluded by design.
- **Ordering — before foam:** Translucency is added before `mix(..., foam_amount)` so foam correctly overwrites it on breaking crests.
- **Scale 0.15:** Keeps the effect subtle relative to the specular highlight. Can be promoted to a `WaterInfos` uniform later if per-scene tuning is needed.
- **No new WaterInfos parameters:** Consistent with the issue's "first iteration" scope; scale is hardcoded as specified.

## Output / notes for future features

- The 0.15 scale and `kTipColor` are inline constants — a follow-up can expose them via `WaterInfos` if artistic control is required.
- The effect is gated by `sun_vis` (sun above the horizon), so it disappears correctly at night.
- `back_scatter * steepness` naturally confines the glow to near-vertical crest faces — flat open-ocean water is unaffected.

## Skills / instructions followed

- `data/shaders/glsl/CLAUDE.md`: equation documentation in header comments, `#include` for uniforms.
- `CLAUDE.md` (project): git workflow (checkout dev → pull → branch → implement → cpplint → commit → PR).
- Issue #358 specification followed verbatim for formula, color constant, and scale.
