# Screen-Space Refraction with Beer-Lambert Absorption

**Issue**: #355
**Branch**: feat/environment-screen-space-refraction-355
**Milestone**: Realistic Water Rendering

## Summary

Upgraded the water forward pass shader (`water_ps.glsl`) with physically correct
screen-space refraction and Beer-Lambert light absorption for the underwater scene.

## Changes

### `data/shaders/glsl/water/water_ps.glsl`

Three independent improvements to the refraction pipeline:

#### 1. Tangent-space distortion (was world-space)

**Before**: `distortion = N.xz * refraction_str * max(1.0 - water_depth * 0.5, 0.0)`
**After**: `distortion = ts_n.xy * refraction_params.x`

Using the tangent-space normal `ts_n.xy` instead of the world-space `N.xz` produces
screen-aligned distortion that correctly tracks the ripple direction regardless of camera
angle. The old depth-based attenuation (`max(1.0 - water_depth * 0.5, 0.0)`) suppressed
refraction in deep water, which is physically wrong — the distortion should be driven only
by surface curvature, while depth controls colour absorption.

#### 2. Sky-pixel safety check

After computing `refract_uv`, the shader now checks whether that UV lands on a sky
fragment (`depth >= 0.9999`). If so, it falls back to `screen_uv` to avoid sampling
sky colour through the water surface at grazing angles near the horizon.

#### 3. World-space depth reconstruction (was linearized view-space)

**Before**: depth computed as difference of linearized depth buffer values.
**After**: the scene bottom world position is reconstructed via `inv_view_proj * ndc`,
and `water_depth = max(v_world_pos.y - world.y/world.w, 0.0)`.

This gives depth in world-space metres, which is the correct unit for Beer-Lambert
absorption. The old linearized approach gave view-space units and was affected by
camera orientation, producing inconsistent absorption at oblique viewing angles.
Sky fragments (no geometry below water) are treated as 100 m depth.

#### 4. Beer-Lambert formula correction

**Before**: `absorption = 1.0 - exp(-d * k)` → `mix(refracted, water_color, absorption)`
**After**: `absorption = exp(-d * k)` → `mix(water_color, refracted, absorption)`

`exp(-d * k)` is the transmission factor (1 = full transmission at surface, 0 = opaque
at depth). The previous code used the complementary absorption factor with a reversed
`mix` operand order, which produced the same result algebraically but obscured the
physical interpretation. The corrected form reads as: "blend from intrinsic water colour
(deep) to refracted scene (shallow)."

## Parameters (unchanged, already in `WaterInfos`)

| Field | Default | Meaning |
|-------|---------|---------|
| `refraction_params.x` | 0.03 | Refraction strength — scales `ts_n.xy` distortion |
| `refraction_params.y` | 0.20 | Absorption scale — water fully tinted at ~5 m depth |

## Decisions

- **Removed `LinearizeDepth` helper**: now that depth is reconstructed via
  `inv_view_proj`, the linearization helper is no longer used anywhere and was deleted
  to keep the shader clean.
- **Depth sampled at `screen_uv`, not `refract_uv`**: Beer-Lambert depth should reflect
  the actual column of water the eye ray passes through, not the distorted position.
- **Clamp to `[0.001, 0.999]`** (not `[0.0, 1.0]`): avoids texel bleed at the very edge
  of the screen framebuffer.

## Skills / CLAUDE.md rules used

- `data/shaders/glsl/CLAUDE.md`: custom `#include` for uniforms, Blender GLSL style,
  equation documentation in comments.
- `CLAUDE.md` (project): conventional commits, history file, cpplint (N/A for GLSL).
- `src/CLAUDE.md`: include-root convention (for understanding uniform includes).

## Notes for future work

- The `refraction_params.x` default of 0.03 may need tuning once the engine runs at
  higher resolutions — it is expressed in UV space, so the apparent distortion scales
  with resolution.
- If a separate refraction pass or underwater post-effect is added later, the
  `water_depth` value computed here (world-space metres) is the right input to use.
