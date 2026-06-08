# Foliage Bent Normals

## Problem

Foliage was rendered very dark. Grass sprites are upright quads whose geometric
normals point horizontally (XZ plane). With a sun/directional light coming from
above, `dot(N_geo, L) ≈ 0`, yielding near-zero diffuse contribution and
nearly-black foliage regardless of light intensity.

## Solution

Introduced **bent normals** in `foliage_vs.glsl`: the geometric normal is
blended toward world-up `(0, 1, 0)` before being written to the G-buffer.

```
N_bent = normalize(lerp(N_geo, up, BENT_FACTOR))   — BENT_FACTOR = 0.6
```

This is the standard game-industry technique for foliage sprites (used in
Unreal Engine and Unity foliage systems). A factor of 0.6 means sprites still
retain 40 % of their geometric normal contribution while the dominant sky/sun
light now correctly illuminates horizontal foliage surfaces.

No G-buffer layout changes were needed; the bent normal fits the existing
`out_normal` RGBA16F channel and the standard deferred lighting passes consume
it transparently.

## Files Changed

| File | Change |
|------|--------|
| `data/shaders/glsl/foliage/foliage_vs.glsl` | Blend geometric normal toward world-up with `BENT_FACTOR = 0.6` |

## Decisions & Rationale

- **Vertex shader only**: The fix lives entirely in the VS, keeping the G-buffer
  layout and the lighting pass untouched. No additional render targets or flags.
- **BENT_FACTOR = 0.6**: Empirically chosen. Too low (< 0.4) barely helps; too
  high (> 0.8) makes all foliage look identically top-lit regardless of
  geometry. 0.6 gives a good balance.
- **Billboard pass unchanged**: Billboards render in the emissive pass with a
  fixed 0.7 dimming factor, bypassing deferred lighting entirely — no change
  needed there.

## Future Improvements

- **Wrap lighting**: A per-fragment `(dot(N,L) + wrap) / (1 + wrap)` term
  stored in `out_normal.w` (currently unused) could add a softer,
  subsurface-like glow to foliage, complementing the bent normals.
- **Configurable BENT_FACTOR per layer**: Expose `bent_factor` in
  `FoliageLayerDesc` and pass it as a uniform so dense canopy foliage can use
  a different value than sparse ground cover.

## Skills / Instructions Used

- `CLAUDE.md` git workflow (branch from dev, conventional commit, PR)
- `data/shaders/glsl/CLAUDE.md` shader naming and include conventions
