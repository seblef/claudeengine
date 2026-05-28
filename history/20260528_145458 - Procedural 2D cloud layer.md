# Procedural 2D Cloud Layer — Issue #324

## Summary

Added a `CloudRenderer` singleton that projects a scrolling FBM noise field
onto a virtual cloud plane and alpha-blends the result over the Preetham sky.
Clouds scroll according to the WindSystem vector and are controlled by a single
`cloud_density` parameter (0 = clear, 1 = overcast).

## Files changed

| File | Change |
|---|---|
| `src/environment/CloudRenderer.h` | New Singleton header |
| `src/environment/CloudRenderer.cpp` | New implementation |
| `src/environment/CMakeLists.txt` | Added `CloudRenderer.cpp` to the `environment` library |
| `src/renderer/Renderer.h` | Forward-declared `CloudRenderer`; added `SetCloudRenderer()` / `SetCloudDensity()` / `cloud_renderer_` / `cloud_density_` |
| `src/renderer/Renderer.cpp` | Included `CloudRenderer.h`; inserted cloud pass (step 3b) after the sky pass |
| `data/shaders/glsl/clouds/clouds_vs.glsl` | New vertex shader (passthrough clone of `sky_vs`) |
| `data/shaders/glsl/clouds/clouds_ps.glsl` | New fragment shader (FBM noise + density threshold + alpha blend) |

## Design decisions

### View-ray projection onto a flat cloud plane

The fragment shader intersects the view ray with a horizontal plane at
`eye_pos.y + kCloudAltitude` (800 m).  This avoids a full atmospheric
ray-marching loop while still producing perspective-correct UV coordinates:
clouds look larger near the horizon (longer ray travel) and smaller at the
zenith, matching the natural appearance of a flat overcast deck.

### FBM with value noise over Perlin noise

Value noise has a cheaper `fract(p.x * p.y)` hash, avoids a gradient table,
and produces softer cloud shapes suitable for a 2D projection.  Four octaves
give enough detail without measurable GPU cost on the fullscreen quad.

### Density threshold via `smoothstep`

The `cloud_density` uniform controls coverage by shifting the lower bound of
a `smoothstep` applied to the raw FBM output.  A fixed edge width of 0.15 UV
units prevents hard aliased contours at all coverage levels.

### Horizon fade

A `smoothstep(0.0, 0.08, view_dir.y)` multiplier fades clouds near the
horizon.  This hides the abrupt plane–ray intersection that would appear as a
sharp ring around the camera.

### Render order (step 3b)

Clouds are drawn *after* the sky (step 3) and *before* the geometry pass
(step 4).  Depth state is the same LEQUAL / write-off configuration used by
the sky pass, so clouds only paint background pixels (G-buffer depth == 1.0)
and are automatically hidden by any foreground geometry.

### world_time parameter not yet used by clouds

The `Render(world_time, cloud_density)` signature mirrors the other environment
renderers but the shader currently derives scroll only from `wind_time` (slot 7
WindInfos CB).  `world_time` is reserved for future features (e.g. reducing
cloud density at night or tinting clouds at sunset).

## Constants (shader-side)

| Constant | Value | Role |
|---|---|---|
| `kCloudAltitude` | 800 m | Height of the virtual cloud plane |
| `kCloudScale` | 400.0 | World-unit size of one noise tile |
| `kCloudSpeed` | 0.00003 | UV drift per second per m/s of wind |

## Notes for future contributions

- **3D lighting**: shade clouds using `sun_direction` from `SkyInfos` (slot 8) — add a simple dot-product darkening on the underside.
- **Cloud colour tint**: lerp from pure white (midday) toward orange/red (sunset) based on `time_of_day`.
- **Editor integration**: expose `cloud_density` in `EnvironmentDesc` and add an IMGUI slider in the editor environment panel.
- **DX12 port**: `SetUniformFloat` will need the equivalent DX12 root-constant or CB path.

## Skills / CLAUDE.md guidance followed

- `src/CLAUDE.md` — one class per `.h`/`.cpp`; Google C++ style; include root `src/`
- `src/environment/CLAUDE.md` — no platform/OpenGL headers in environment; one-way dependency (`renderer` → `environment`)
- `data/shaders/glsl/CLAUDE.md` — `_vs` / `_ps` suffix convention; `#include <uniforms/…>` for shared UBOs
- `src/CLAUDE.md` — new module needs entry in `src/environment/CMakeLists.txt`
