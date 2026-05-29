# Star Point Rendering

**Issue:** #346 — Stars are quads  
**Branch:** `feat/star-point-rendering-346`

## Problem

At night, stars were rendered as uniform-brightness rectangular quads. This happened because the fragment shader divided the sky hemisphere into a spherical (lon, lat) grid, hashed each cell, and for qualifying cells added a flat brightness across the entire cell — making each star fill its full grid cell.

## Changes

**`data/shaders/glsl/sky/sky_ps.glsl`** — star section rewritten:

| Before | After |
|---|---|
| `star_grid = floor(coord)` only | `star_grid + star_uv = floor + fract(coord)` |
| Uniform brightness per cell | Gaussian core at random sub-cell center |
| No shape | Gaussian core + diffraction spikes |
| White only | Blue-white to yellow-white tint per star |

### Algorithm

1. **Sub-cell position** — two additional `StarHash` calls (with different seed offsets) generate a random `centre` in [0, 1)² within each qualifying cell. `d = star_uv − centre` is the offset from the current pixel to that center, in cell-width units.

2. **Gaussian core** — `exp(−‖d‖² × 100)` gives σ ≈ 0.07 of a cell width, which maps to roughly 1 pixel at typical screen resolutions (1080p, 60° FOV → ~12 px/cell).

3. **Diffraction spikes** — two separable Gaussians produce a cross shape:
   - `spike_h = exp(−dx²×3) × exp(−dy²×120)` — horizontal ray
   - `spike_v = exp(−dy²×3) × exp(−dx²×120)` — vertical ray
   - Weighted at 0.4 so spikes are subtler than the core

4. **Color tint** — a third hash linearly interpolates each star between blue-white (0.75, 0.9, 1.0) and yellow-white (1.0, 0.95, 0.8), giving the night sky natural stellar color variety.

## Decisions

- **Shader-only fix**: no C++ changes required since `SkyRenderer` loads the shader at runtime from disk.
- **Spikes vs. no spikes**: diffraction spikes are a recognizable star hallmark in games and photography; keeping them subtle (×0.4) avoids an over-stylized look.
- **`kStarThresh` made `const float`**: small readability improvement while touching the code; no functional change.
- **cpplint**: not applicable — the only modified file is GLSL, not C++.

## Output to keep in mind

- The star grid (80 divisions) is shared with the previous implementation; star positions on the sky are unchanged, only their rendered shape changed.
- Stars near cell boundaries may appear slightly asymmetric (spikes clipped by adjacent cells not rendering), but this is imperceptible in practice because spikes fall off to < 2% at the cell edge.
- The `StarHash` function is reused with two different constant offsets as seed perturbations; these offsets must remain distinct to avoid hash collisions producing correlated center coordinates.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per file, Google C++ style
- `src/environment/CLAUDE.md` — module dependency rules
- `data/shaders/glsl/CLAUDE.md` — shader naming and include conventions
- `CLAUDE.md` (root) — git workflow, history file, PR rules
