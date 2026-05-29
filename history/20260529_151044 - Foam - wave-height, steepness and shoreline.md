# Foam — Wave-Height, Steepness and Shoreline (#356)

## Summary

Added three physically-motivated foam sources to the water renderer, all
modulated by a procedural tileable foam texture:

1. **Wave-height foam** — activates when a Gerstner crest rises above
   `foam_height_thresh` (default 0.60 m above the resting water plane).
2. **Steepness foam** — activates when the Gerstner macro normal tilts far
   from vertical (breaking-wave geometry), controlled by
   `foam_steepness_thresh` (default 0.70).
3. **Shoreline foam** — an animated ring following the depth contour
   `foam_shoreline_depth` (default 2.0 m), with oscillation driven by
   `foam_speed` (default 1.5 rad/s).

All three sources are combined with `max()`, then modulated by two scrolling
samples of a procedural foam blob texture (binding 1) and clamped to [0, 1].
The final surface colour is linearly blended toward white (`mix(…, vec3(1),
foam_amount)`).

## Files changed

| File | Change |
|---|---|
| `src/environment/WaterRenderer.h` | Added `foam_tex_` member, `BuildFoamTexture()` declaration; updated class doc comment |
| `src/environment/WaterRenderer.cpp` | Added `kFoamTexSize`, `FoamValue()`, `BuildFoamTexture()`; bind slot 1 in `Render()`; reset `foam_tex_` in `Reset()` |
| `data/shaders/glsl/water/water_ps.glsl` | Added `u_foam_tex` sampler (binding 1); replaced single-source foam with three-source foam + texture modulation; added foam→white colour blend |

## Decisions

### Procedural foam texture

The texture generator uses the **product of four overlapping sine terms**
(each mapped to [0, 1]) to produce sparse, isolated bright blobs.  The
product is then scaled by 6× and clamped; this boosts the sparse peaks into
clearly visible white patches while keeping background regions dark.  The
two-sample double-product in the shader creates further natural variation
without a second texture asset.

The choice of ×6 scaling comes from the typical mean of the product
distribution (~0.0625 for four independent uniform [0, 1] variables),
mapping the 80th-percentile region cleanly into [0, 1].

### Existing shoreline simple foam replaced

The previous single-line shoreline foam
(`1 - smoothstep(0, depth_thresh, water_depth)`) was replaced entirely by
the three-source composite.  The new shoreline source replicates and extends
that behaviour with an animated ring oscillation.

### Sampler slot 1

Sampler slot 0 was already taken by the normal map.  Slots 2 and 3 are the
scene-colour and depth snapshots.  Slot 1 was the only free adjacent slot,
consistent with the issue spec.

### `v_uv` as foam texture coordinate

The vertex shader passes `in_position.xz` as `v_uv` (world XZ), so the foam
texture UV offsets `v_uv * 0.04` and `v_uv * 0.07` map to world-space tile
sizes of ~25 m and ~14 m respectively, giving visible foam clumps at a
plausible ocean scale.

## Parameters (all pre-existing in WaterInfos, unchanged defaults)

| Field | Location in UBO | Default |
|---|---|---|
| `foam_height_thresh` | `refraction_params.z` | 0.60 m |
| `foam_shoreline_depth` | `refraction_params.w` | 2.0 m |
| `foam_steepness_thresh` | `foam_params.x` | 0.70 |
| `foam_speed` | `foam_params.y` | 1.5 |

## Output to keep in mind

- The foam texture blob density is controlled by the `* 6.0f` scale factor in
  `BuildFoamTexture()`.  A larger factor → more coverage; smaller → sparser clumps.
- The double-product `ft = s1 * s2` attenuates foam significantly (to ~25% on
  average); the compensating `* 2.5` factor restores full brightness at peaks.
  Tuning either factor changes coverage vs contrast.
- `v_uv` is world XZ, not normalised [0, 1].  All UV scales in the foam sampling
  code must be chosen relative to world units, not texture space.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md` (C++ style guide, one class per file, project-relative includes)
- `src/environment/CLAUDE.md` (module dependency rules, one class per .h/.cpp pair)
- `data/shaders/glsl/CLAUDE.md` (shader naming convention, #include for uniforms)
- CLAUDE.md git workflow (branch from dev, conventional commits, PR to dev)
