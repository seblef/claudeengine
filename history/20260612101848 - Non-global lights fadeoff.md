# Non-global lights smooth fadeoff (fix #503)

## Problem

Omni, circle-spot and rect-spot lights rendered a hard edge at the boundary of their
geometry (sphere / cone / pyramid). This was caused by the inverse-square attenuation
formula:

```
atten = 1.0 / (1.0 + dist² / range²)
```

At `dist = range` this evaluates to **0.5**, not 0. Because the rasteriser clips the
light geometry exactly at `range`, the non-zero residual value created a visible sharp
ring wherever the geometry silhouette was clipped.

## Fix

Added a range-based edge fadeoff to all three pixel shaders using `smoothstep`:

```glsl
float d_norm    = clamp(dist / range, 0.0, 1.0);
float edge_fade = 1.0 - smoothstep(0.8, 1.0, d_norm);
```

- Inside **80 %** of range: `edge_fade = 1.0` — no change to existing lighting.
- Between **80 %** and **100 %** of range: smooth cubic fade to 0.
- At `dist = range`: `edge_fade = 0.0` — light contribution is exactly zero.

The 20 % soft-edge window was chosen to match the existing convention used by the H/V
falloff in `rect_spot_ps` (`smoothstep(angle * 0.8, angle, px)`).

## Files changed

| File | Change |
|------|--------|
| `data/shaders/glsl/lighting/omni_light_ps.glsl` | Added `edge_fade`; multiplied into `out_color` |
| `data/shaders/glsl/lighting/circle_spot_ps.glsl` | Added `edge_fade`; multiplied into `out_color` |
| `data/shaders/glsl/lighting/rect_spot_ps.glsl` | Added `edge_fade`; multiplied into `out_color` |

## Decisions & rationale

- **smoothstep over polynomial window**: `smoothstep` is already used in the codebase
  for angular falloffs; keeping the same primitive ensures consistent visual language
  and avoids introducing a new function.
- **0.8 threshold**: mirrors the 80 % convention already used for H/V falloffs in the
  rect-spot shader. Keeps all three lights visually consistent.
- **No C++ changes needed**: the fix is purely in the shaders. The geometry that
  determines the light volume boundary is unchanged; we are just making the pixel
  contribution fade to zero before reaching it.

## Keep in mind for future work

- If light range behaviour needs to be artist-configurable (e.g. expose the fade width
  as a parameter), it would require adding a new field to `light_infos.glsl` UBO and
  the corresponding C++ uniform upload.
- Global lights (directional / sky) are unaffected — they use separate shaders with no
  range concept.

## Skills / CLAUDE.md instructions applied

- `data/shaders/glsl/CLAUDE.md`: comment equations, follow Blend GLSL style, use
  `#include` for shared uniforms.
- Root `CLAUDE.md`: branch from `dev`, conventional commit, PR to `dev`.
