# feat(clouds): domain warping for procedural cloud shader (#549)

## Summary

Applied Inigo Quilez's domain warping technique to `data/shaders/glsl/clouds/clouds_ps.glsl` to eliminate the visually repetitive, grid-aligned patterns produced by plain 4-octave FBM.

## Changes

**`data/shaders/glsl/clouds/clouds_ps.glsl`**
- Added `kWarpScale = 0.8` constant controlling warp displacement strength.
- Replaced the single `density = FBM(uv)` call with a two-step domain warp:
  1. Compute `warp = vec2(FBM(uv), FBM(uv + vec2(5.2, 1.3)))` — two decorrelated FBM samples.
  2. Feed `uv + kWarpScale * warp` into the density FBM.
- Updated the file-level comment to document the domain warping and its performance cost.

## Decisions & Rationale

| Decision | Rationale |
|---|---|
| Reuse existing `FBM()` unchanged | No helper refactor needed; the function is stateless and accepts any `vec2`. |
| Fixed offset `(5.2, 1.3)` for Y-axis warp | Standard Quilez constants; shifts the sample far enough in noise space to fully decorrelate the two warp axes. |
| `kWarpScale = 0.8` | Matches the value suggested in the issue; gives clearly swirling formations without over-distorting cloud edges. |
| Named constant `kWarpScale` instead of a magic literal | Follows the existing constant-naming convention in the file and makes tuning explicit. |

## Performance impact

One extra `FBM()` call (4 `ValueNoise` samples + 4 `Hash2` calls) is added per fragment. For a sky-plane covering a typical viewport this is acceptable; the plain FBM path runs 8 value-noise samples and the warped path runs 12.

## What to keep in mind for future features

- `kWarpScale` is the primary artistic dial. Values in `[0.5, 1.2]` cover the useful range; beyond `~1.5` density features become too smeared.
- A second-level warp (`density = FBM(uv + warp + FBM(warped_uv))`) would add more complexity at the cost of another FBM pass — only worth it if cloud detail is a priority and fragment cost is acceptable.
- The `(5.2, 1.3)` decorrelation offset is arbitrary; any large irrational-looking offset works.

## Skills / CLAUDE.md references used

- `impl-issue` skill — workflow (checkout dev, branch, implement, commit, PR).
- `data/shaders/glsl/CLAUDE.md` — shader naming and `#include` conventions.
- `CLAUDE.md` (root) — conventional commits format, history file requirement.
