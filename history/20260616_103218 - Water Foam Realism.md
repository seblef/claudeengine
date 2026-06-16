# Water Foam Realism — Issue #554

## Summary

Three targeted changes improve the visual realism of water surface foam without
any runtime performance cost.

## Changes

### `src/environment/WaterRenderer.cpp` — `BuildFoamTexture` / `FoamValue`

**Problem**: The old formula multiplied four sine waves (`a*b*c*d`). Bright
peaks only appeared where all four coincided, producing perfectly round,
sparse, isolated blobs — nothing like real sea foam.

**Decision**: Replace with **Worley (cellular) F1 noise**, baked once at
startup into the 256×256 tileable texture.

Implementation:
- `kWorleyCells = 8` divides the 256×256 texture into an 8×8 grid of 32 px
  cells, each containing one randomly placed seed point via `WorleyHash`.
- `FoamValue(u, v)` computes the minimum toroidal distance to any point in
  the 3×3 cell neighbourhood, returning `(1 − dist_normalised)²` — bright at
  seed points, dark in between, with squared sharpening for distinct bubble edges.
- Toroidal tileability: neighbouring cells use modulo-wrapped indices for the
  hash lookup but un-wrapped offsets for the distance, so the pattern wraps
  seamlessly without explicit `if` guards.
- The 6× brightness boost in `BuildFoamTexture` was removed: Worley values
  already span [0, 1] naturally.

### `data/shaders/glsl/water/water_ps.glsl` — foam texture sampling

**Problem**: `ft = s1 * s2` collapses two texture samples multiplicatively,
making foam extremely sparse and binary.

**Decision**: Weighted blend `ft = s1 * 0.6 + s2 * 0.4`.  The large-scale
pass (UV × 0.04) contributes 60% and the fine-detail pass (UV × 0.07) adds
40%, giving richer multi-scale foam patches.

### `data/shaders/glsl/water/water_ps.glsl` — foam color

**Problem**: `mix(final_color, vec3(1.0), foam_amount)` produces hard pure
white, which looks plastic and artificial.

**Decision**: `mix(final_color, vec3(0.92, 0.95, 0.98), foam_amount * 0.85)`.
The off-white tint (slightly blue-shifted, as real sea foam appears) at 85%
opacity ensures the underlying water color is always subtly visible through
the foam, matching the partly translucent nature of real bubbles.

## Trade-offs considered

- **Worley cell size (32 px)**: smaller cells → denser bubbles; larger →
  coarser patches. 8×8 at 256×256 gives medium clusters that look natural
  and are broken up further by the two-pass UV scrolling at different scales
  in the shader, preventing visible tiling.
- **Squared Worley value**: sharpens the bright peak around each seed point.
  A linear falloff would look smoother but more like a soft haze than bubbles.
- **85% opacity cap**: a value of 1.0 would completely mask the water colour
  under foam; 85% is the minimum that still reads as opaque foam while
  allowing a hint of water tint on wave crests.

## Files changed

- `src/environment/WaterRenderer.cpp`
- `data/shaders/glsl/water/water_ps.glsl`

## Skills and CLAUDE.md sections used

- `impl-issue` skill
- `src/CLAUDE.md` — Google C++ style, one class per file, cpplint
- `src/environment/CLAUDE.md` — module dependency rules
- `data/shaders/glsl/CLAUDE.md` — GLSL coding style, comment equations
- Root `CLAUDE.md` — git workflow, conventional commits, history file
