# Fix: Water Sun Reflections Wrong Direction (#473)

## Problem

The Cook-Torrance GGX specular highlight on water appeared at the **opposite** horizontal direction from the visible sun. Looking toward the sun produced no highlight; looking away did.

## Root Cause

In `src/renderer/Renderer.cpp`, `UpdateWaterRenderer()` computed a simplified sun direction vector:

```cpp
// BUG: both X and Z were negated
const core::Vec3f sun_dir(
    -std::cos(azimuth) * cos_el,   // ← wrong sign
     std::sin(elev),
    -std::sin(azimuth) * cos_el);  // ← wrong sign
```

With engine coordinates `(+X east, +Y up, +Z south)` and `azimuth = 0` at sunrise:
- `t=6` (sunrise/east): formula produced `(-cos_el, …, 0)` → pointing **west** (-X), but the sun is in the **east** (+X)
- `t=12` (noon/south): formula produced `(0, …, -cos_el)` → pointing **north** (-Z), but the sun is in the **south** (+Z)

The negation on both components rotated the sun direction 180° in the horizontal plane.

## Fix

Remove the spurious negation from X and Z:

```cpp
// FIX
const core::Vec3f sun_dir(
     std::cos(azimuth) * cos_el,
     std::sin(elev),
     std::sin(azimuth) * cos_el);
```

This aligns `UpdateWaterRenderer()`'s convention with the coordinate system used by `SkyRenderer::ComputeSunDirection()`.

## Impact on the Water Shader

The water fragment shader (`water_ps.glsl`) uses:
```glsl
vec3  H = normalize(sun_dir + V);   // Cook-Torrance half-vector
float n_dot_l = max(dot(N, sun_dir), 0.0);
```

With the correct `sun_dir` (pointing toward the sun), `H` peaks when `V` is on the same side as the sun (camera facing the sun), producing the specular glint in the correct direction.

## Files Changed

- `src/renderer/Renderer.cpp` — removed negation from `sun_dir.x` and `sun_dir.z` in `UpdateWaterRenderer()`

## Skills Used

- `impl-issue`

## CLAUDE.md Notes Followed

- Git workflow: branched from `dev`, conventional commit, PR to `dev`
- `cpplint` run before commit — no issues
- One-line fix, no extraneous changes
