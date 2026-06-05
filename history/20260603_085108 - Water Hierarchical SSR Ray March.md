# Water: Hierarchical SSR Ray March

**Issue:** #383  
**PR:** #387  
**Branch:** `feat/water-hierarchical-ssr-383`  
**Date:** 2026-06-03

## Changes

Single file modified: `data/shaders/glsl/water/water_ps.glsl`

Replaced the 32-step × 4 m linear SSR ray march with a two-phase hierarchical approach.

### Before
```
const int   kSteps    = 32;
const float kStepSize = 4.0;
// 32 depth texture samples per fragment in the worst case
```

### After
- **Phase 1 — coarse bracketing**: 8 steps × 16 m = 128 m max reach. Same reach as before. Preserves `clip.w ≤ 0` and `|ndc.xy| > 1.05` early-outs.
- **Phase 2 — binary refinement**: 4 bisection iterations over the bracketed `[hit_step-1, hit_step]` coarse interval (one `depth_tex` sample per iteration). Final colour sample at refined `t_hi`.
- Total samples in the hit case: 8 (coarse) + 4 (refine) + 1 (colour) = **13** vs. up to 32+1 before.
- No-hit case: ≤ 8 depth samples (with early-out) vs. up to 32.

## Decisions & Rationale

- **Step sizes**: 8 × 16 m matches the original 128 m reach exactly. Coarser steps mean no hits for thin geometry narrower than 16 m, but the binary refinement recovers sub-metre accuracy within any bracket that does fire.
- **`dist_fade` formula**: changed from `1 - i/32` (integer step index) to `1 - t_hi/128.0` (continuous metres). Semantically identical for the original linear march; marginally smoother for the refined position.
- **Removed the 0.02 NDC thickness guard** from Phase 1 (it was `ray_depth > stored_depth && ray_depth - stored_depth < 0.02`). Phase 1 only needs to detect the crossing; Phase 2 then converges to the surface boundary naturally, making the thickness guard unnecessary.
- **kMaxReach constant**: `kCoarseStep * float(kCoarseSteps)` computed as a GLSL constant so the compiler can fold it. 

## Skills & CLAUDE.md instructions used

- `impl-issue` skill
- Project CLAUDE.md: git workflow (checkout dev → branch → implement → cpplint → commit → PR)
- `data/shaders/glsl/CLAUDE.md`: comment equations, follow Blend GLSL style, use `#include` for uniforms

## Output to keep in mind

- The water pass now has a hard asymmetry: Phase 1 misses geometry thinner than ~16 m in world space. If very thin objects (fences, vegetation) are ever placed underwater this could cause missed reflections.
- Binary refinement assumes the depth function is monotone in the bracket (no double-sided transparent geometry). This matches the deferred depth buffer which stores the nearest opaque surface.
- If a future feature needs higher SSR precision, increasing refinement iterations from 4 to 5–6 costs only one extra `depth_tex` sample and narrows the hit band to ≤ 0.5 m.
