# Foliage Billboard Lighting

## Problem

Foliage billboards (far LOD) were rendered with a fixed `0.7` dimming factor,
completely ignoring all lights. Near-mesh instances go through the deferred
G-buffer and receive full Blinn-Phong lighting, so the mesh↔billboard transition
was jarring — especially when the global light intensity or direction changed
(time-of-day, weather).

## Solution

Applied a **forward-pass directional-light approximation** to the billboard
pixel shader, mirroring the equation used by `global_light_ps.glsl`:

```
L    = normalize(-direction)          // surface-to-light (same convention as deferred pass)
diff = max(dot(world_up, L), 0)       // world-up normal, same as mesh bent-normal direction
lit  = albedo * (ambient + color * intensity * diff)
```

Using world-up `(0, 1, 0)` as the billboard normal directly mirrors the 60 %
bent-normal bias applied to mesh instances (`feat/foliage-bent-normals`), so
both LOD tiers respond identically to top-down lighting.

### LightInfosBlock binding issue

`LightRenderer::Render()` uses a single `light_infos_cb_` (UBO binding 4) that
is overwritten by every light draw call. By the time the emissive pass runs,
the buffer holds the last *local* light's data, not the global light.

Fix: added `LightRenderer::BindGlobalLight()`, which re-fills the buffer with
the first GlobalLight's data. `Renderer::Render()` calls it once before any
billboard draw, making binding 4 reliable for forward-pass shaders.
`TreeRenderer::RenderBillboards` benefits automatically when its shader is
updated.

## Files Changed

| File | Change |
|------|--------|
| `data/shaders/glsl/foliage/foliage_billboard_ps.glsl` | Replace fixed dim with forward directional-light calculation |
| `src/renderer/LightRenderer.h` | Declare `BindGlobalLight()` |
| `src/renderer/LightRenderer.cpp` | Implement `BindGlobalLight()` |
| `src/renderer/Renderer.cpp` | Call `BindGlobalLight()` before billboard emissive pass |

## Decisions & Rationale

- **World-up normal**: Matches the bent-normal direction of mesh instances
  (BENT_FACTOR = 0.6 strongly biases toward up). Both tiers react identically
  to the sun angle.
- **Global light only**: Point and spot lights are not applied to billboards.
  At billboard distances (40 m+) the contribution of local lights is negligible,
  and the main perceptual mismatch was the global sun light.
- **Single `BindGlobalLight()` call**: One UBO fill per frame regardless of how
  many billboard layers exist. Negligible CPU cost.

## Future Improvements

- **Option B (G-buffer integration)**: Move billboards to the geometry pass for
  full deferred lighting (point lights, shadows). The hard alpha cutout makes
  this feasible without blending complications.
- **Cross-fade at transition distance**: A distance-based fade factor in
  `[billboard_distance, billboard_distance + margin]` could further smooth the
  LOD seam if residual differences remain.

## Skills / Instructions Used

- `CLAUDE.md` git workflow (branch from dev, conventional commit, PR)
- `data/shaders/glsl/CLAUDE.md` shader naming and include conventions
- `src/CLAUDE.md` one class per file, Google C++ style
