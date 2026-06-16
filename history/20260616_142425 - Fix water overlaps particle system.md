# Fix: Water overlaps particle system (#587)

## Summary

Fixed a forward-pass rendering order bug where water was rendered on top of
above-water particle effects (fire, smoke, etc.).

## Root Cause

The forward particle pass (`kAdditive` / `kAlphaBlend` emitters) ran at step
4b, **before** the water pass at step 4c.

Forward particles render with **depth test ON / depth write OFF**: they are
correctly occluded by opaque geometry, but do not update the depth buffer.

The water pass uses `kLessEqual` depth test against the G-buffer depth
(opaque geometry only). At a screen pixel covered by a particle, the
G-buffer depth is the opaque surface behind the particle — not the particle
itself. The water surface (between camera and that opaque surface) therefore
passed the depth test and was alpha-blended over the already-rendered
particle. Result: above-water particles were invisible wherever water
occupied the same screen real-estate.

## Fix

Swapped the render order in `Renderer::Update()`:

```
Before:  4b. Particle forward pass  →  4c. Water forward pass
After:   4b. Water forward pass     →  4c. Particle forward pass
```

With particles rendered **after** water, they are composited on top of the
water surface — which is the correct draw order for above-water effects.

The scene colour snapshot taken for water refraction is unaffected: it is
captured before particles (correct, because refraction shows underwater
content, not above-water particles).

The particle forward pass now explicitly binds and unbinds the emissive FBO
for its rendering, since it no longer inherits the FBO binding from the
emissive pass.

## Files Changed

| File | Change |
|---|---|
| `src/renderer/Renderer.cpp` | Reordered steps 4b/4c; particle block now explicitly binds/unbinds the emissive FBO. |
| `src/renderer/Renderer.h` | Updated pipeline comment to document corrected order and rationale. |

## Decisions & Rationale

- **Render order swap vs. depth pre-pass for particles**: A depth pre-pass
  for transparent particles would break semi-transparency (first particle at
  a pixel writes depth, occluding all others). Swapping render order is the
  standard approach in hybrid deferred renderers.
- **No change to the SSR/refraction snapshot**: The snapshot is taken
  before particles in both the old and new pipelines. No correctness
  regression for refraction.
- **Edge case — underwater particles**: Particles below the water level
  will still pass the depth test (the depth buffer only contains opaque
  geometry) and appear above the water surface. This is a pre-existing
  limitation of not having depth writes on forward particles. It can be
  addressed separately if needed.

## Output to Keep in Mind

- The water pass **must** run before any forward-transparent pass whose
  objects can appear above the water level.
- If other forward-transparent objects (foliage alpha cards, glass, etc.)
  are added in the future, they should also be scheduled **after** the
  water pass for the same reason.
- The emissive FBO bind/unbind must be explicit for every forward pass that
  follows the water renderer, because `WaterRenderer::Render()` internally
  switches FBOs and Renderer leaves the FBO unbound after cleanup.

## Skills Used

- `impl-issue`

## Instructions Especially Considered

- `src/CLAUDE.md`: One class per `.h`/`.cpp`, Google style, no extra features.
- Root `CLAUDE.md`: Conventional commit, PR to `dev`, history file required, cpplint before commit.
