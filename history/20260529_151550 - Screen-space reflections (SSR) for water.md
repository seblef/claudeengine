# Screen-Space Reflections (SSR) for Water Surface

**Issue**: #357  
**Branch**: `feat/environment-ssr-357`  
**Date**: 2026-05-29

## Summary

Added screen-space reflection ray marching to the water fragment shader (`data/shaders/glsl/water/water_ps.glsl`). The water surface now reflects visible scene geometry (terrain, objects) instead of falling back entirely to the sky zenith colour.

## Changes

### `data/shaders/glsl/water/water_ps.glsl`

- Added SSR ray march block after Fresnel computation.
- Replaced `surface_col = mix(absorbed, sky_color, fresnel)` with a two-step blend:
  1. `sky_reflect = mix(sky_zenith_color.rgb, reflect_color, ssr_weight)` — SSR hit or sky fallback
  2. `surface_col = mix(absorbed, sky_reflect, fresnel)` — refraction vs. reflection

## Algorithm

**Ray direction**: `R = reflect(-V, N)` using the combined Gerstner + normal-map world-space normal from the existing TBN computation.

**March guard**: Only march when `R.y >= -0.05` (ray goes upward or near-horizontal) and `fresnel > 0.03` (grazing angles where reflection is visible). This skips the loop for fragments seen nearly head-on, saving GPU time.

**Projection**: Each step projects `v_world_pos + R * 4.0 * i` through `view_proj` (already in `SceneInfosBlock`, binding 2) into NDC. Off-screen steps (`|ndc.xy| > 1.05` or `w <= 0`) exit early.

**Intersection test**: `ray_depth > stored_depth && ray_depth - stored_depth < 0.02` (NDC depth space). The 0.02 threshold is a reasonable first approximation; may need tuning for scenes with very different depth distributions.

**Attenuation**:
- `edge_fade`: `1 - smoothstep(0.8, 1.0, |suv*2-1|)` — fades near screen edges to avoid hard cuts.
- `dist_fade`: `1 - i/kSteps` — fades with march distance.
- `ssr_weight = min(edge_fade.x, edge_fade.y) * dist_fade`.

**Fallback**: `reflect_color` initialises to `sky_zenith_color.rgb`; it is only overwritten on a hit, so missed rays (off-screen objects, occluded geometry) gracefully fall back to sky.

## Decisions

- **World-space march vs. screen-space march**: World-space is simpler to implement and avoids the perspective-correct step-size issues of NDC-space marching. The 4 m step gives 128 m reach, sufficient for typical water scenes.
- **No binary-search refinement**: First iteration as specified in the issue. Can be added later if intersection quality is insufficient.
- **No new uniforms or samplers**: `view_proj` was already in `SceneInfosBlock`; `scene_color_tex` (binding 2) and `depth_tex` (binding 3) were already bound for refraction/Beer-Lambert.
- **No C++ changes required**: All needed data was already uploaded by the existing forward water pass infrastructure (#352).

## Output to Keep in Mind

- The depth threshold (0.02 NDC) may need tuning for certain scene scales — a too-small value misses thin geometry, a too-large value causes leaking through thin walls.
- The march is uniform-step; adding a binary search refinement pass would sharpen hit positions at minimal cost.
- 32 steps per fragment is GPU-budget-friendly for water surfaces that typically cover a fraction of the screen.

## Skills / CLAUDE.md Instructions Applied

- `impl-issue` skill workflow: checkout dev → branch → implement → cpplint → commit → PR.
- CLAUDE.md: branch prefix `feat/`, conventional commit message, history MarkDown file required.
- Shader CLAUDE.md: `#include` for uniforms, documentation comments for equations, same-name vertex/pixel suffix convention.
