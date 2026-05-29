# GGX Microfacet Specular for Water

## Summary

Replaced the Blinn-Phong specular model in `water_ps.glsl` with a physically-based Cook-Torrance GGX microfacet BRDF for sun glint on the water surface. This is especially important at grazing angles where Blinn-Phong underestimates the specular lobe and fails to produce the characteristic sun glitter of real water.

## Changes

**`data/shaders/glsl/water/water_ps.glsl`**

- Added `#define PI 3.14159265359` at shader scope.
- Removed the Blinn-Phong block (shininess mapping from roughness, single `pow()` call).
- Replaced with the full Cook-Torrance BRDF:
  - **GGX NDF** (`D`): `alpha2 / (PI * d*d)` — produces a physically narrow, energy-conserving specular peak, especially tight at low roughness (water default 0.05).
  - **Schlick-Smith geometry** (`G`): `G_V * G_L` using the direct-lighting `k = (r+1)²/8` remapping — accounts for self-shadowing and masking of microfacets.
  - **Schlick Fresnel** (`F_spec`): f0 = 0.02, matching water IOR 1.33. This is separate from the view-angle Fresnel used for alpha/reflection blending.
  - Final: `spec_brdf = D * G * F_spec / max(4 * n_dot_v * n_dot_l, 0.001) * n_dot_l`
- Updated header comment to document the new specular equations.

## Decisions and rationale

- **f0 = 0.02**: Water has IOR ≈ 1.33. Schlick f0 = ((n-1)/(n+1))² = (0.33/2.33)² ≈ 0.02. Using the physically correct value instead of a tunable constant.
- **`k = (r+1)²/8`** (direct lighting remapping, not IBL `r²/2`): The sun is a direct light source, not an environment map. This remapping avoids over-darkening at low roughness.
- **`n_dot_l` clamped separately from `sun_vis`**: `sun_vis` is a binary above-horizon gate; `n_dot_l` is the per-fragment cosine for the rendering equation. Both are needed and independent.
- **No change to WaterInfos**: `roughness` (`sky_zenith_color.a`, default 0.05) and `sun_intensity` (`sun_params.w`, default 20.0) were already in place from issue #352.

## Output to keep in mind

- With roughness=0.05 the GGX lobe is very tight — the sun glint will be a sharp, bright highlight rather than a broad bloom. Artists can widen it by increasing roughness.
- The sun_intensity (HDR scale, default 20.0) is multiplied in directly. Tone mapping downstream handles the HDR → LDR conversion; no clamping was added here.
- The `spec` vector feeds directly into `final_color = surface_col + spec`, so it is additive over the Fresnel/refraction surface colour.

## Skills and instructions used

- `impl-issue` skill (issue #354)
- `CLAUDE.md`: git workflow (branch from dev, conventional commits, PR to dev)
- `data/shaders/glsl/CLAUDE.md`: equation documentation in comments, `#include` for uniforms
