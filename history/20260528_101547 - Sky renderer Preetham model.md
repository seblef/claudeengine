# Sky Renderer — Preetham Atmospheric Model

**Branch**: `feat/sky-renderer-preetham-320`
**Issue**: #320
**Date**: 2026-05-28

---

## Changes

### New files

| File | Description |
|------|-------------|
| `src/environment/SkyInfos.h` | Constant buffer struct (slot 8) matching `sky_infos.glsl` std140 layout |
| `src/environment/SkyRenderer.h/.cpp` | Singleton sky renderer (Preetham model, sun disc, moon disc, stars) |
| `data/shaders/glsl/uniforms/sky_infos.glsl` | GLSL UBO declaration for `SkyInfosBlock` (binding 8) |
| `data/shaders/glsl/sky/sky_vs.glsl` | Fullscreen quad vertex shader; reconstructs world-space view ray |
| `data/shaders/glsl/sky/sky_ps.glsl` | Fragment shader: Preetham model, sun/moon disc, star noise |

### Modified files

| File | Change |
|------|--------|
| `src/environment/CMakeLists.txt` | Added `SkyRenderer.cpp` |
| `src/environment/CLAUDE.md` | Documented new classes and updated dependency graph |
| `src/renderer/Renderer.h` | Added `SetSkyRenderer()`, `SetSkyWorldTime()`; documented slot 8 and sky pass |
| `src/renderer/Renderer.cpp` | Added sky pass (step 3) between lighting and emissive passes |
| `src/renderer/CMakeLists.txt` | Linked `environment` as private dependency |

---

## Architecture decisions

### SkyRenderer location: `environment/` not `renderer/`

The issue explicitly places `SkyRenderer` in the environment module, which is
the right domain (it is an environmental effect). However, `renderer/Renderer.cpp`
must call it, creating a `renderer → environment` dependency at the link level.

**Solution**: forward-declare `environment::SkyRenderer` in `Renderer.h` (no
`#include` in the header), and include `"environment/SkyRenderer.h"` only in
`Renderer.cpp`. `renderer` CMakeLists links `environment` as a private dependency.
This keeps the public interface of `Renderer.h` free of environment headers.

### No renderer::GeometryData in environment/

`GeometryData` is a renderer class. To keep `environment → abstract` clean,
`SkyRenderer` creates its fullscreen quad using raw `abstract::VertexBuffer` and
`abstract::IndexBuffer` directly. This avoids adding a `renderer` compile-time
dependency to the environment module.

### Sky render placement in the pipeline

The sky must:
- Write to the HDR render target (emissive FBO).
- Fill **only background pixels** (depth = 1.0 in G-buffer).
- Not interfere with lit geometry.

Rendering between step 2 (lighting pass) and step 4 (emissive pass), as a
dedicated step 3, achieves this:
- G-buffer depth is already established from step 1.
- The emissive FBO is bound; depth test is LEQUAL with depth write OFF.
- No additive blending — sky is a regular colour write (not accumulated).

This avoids the need to enable/disable blending around the sky call.

### World time passing

`Renderer::Update()` takes `float time` (scene timer), not time of day.
`SetSkyWorldTime(float t)` allows the caller to push the time of day (0–24 h)
before calling `Update()`. This follows the same setter pattern as `SetCamera()`.

### Preetham model implementation

The fragment shader implements the Preetham 1999 model faithfully:
- **Zenith luminance** from eq. (7).
- **Zenith chromaticity** from Tables 2 & 3.
- **Perez A–E coefficients** from Table 1 (for Y, x, y vs. turbidity T).
- Normalization at zenith ensures the zenith direction gives exactly `Y_z`.
- Luminance scale factor `0.05` converts from kcd/m² to a display-friendly
  HDR range (~0–2 for midday sky); HDR values > 1 are tone-mapped by the
  composite pass's gamma correction.

Day/night blending uses a smooth 10° band around the solar horizon
(`day_factor = clamp(sun.y * 10 + 0.5, 0, 1)`).

### Sun disc HDR values

The sun disc uses `60, 48, 24` (HDR). After the composite `pow(hdr, 1/2.2)`,
these clamp to white on screen. The emissive pass's additive accumulation means
sky + bloom from other emissive objects works naturally with the existing pipeline.

---

## Output to keep in mind

- **Slot 8** is now dual-use: texture sampler 8 (depth RT in lighting pass) and
  UBO binding 8 (SkyInfos in sky pass). These are separate namespaces in OpenGL
  and do not conflict.
- **`SetSkyWorldTime(float)`** must be called before `Renderer::Update()` each
  frame (e.g., `renderer.SetSkyWorldTime(world_time.GetTimeOfDay())`).
- **`SetSkyRenderer()`** follows the same ownership model as `SetTerrainRenderer()`:
  `Renderer` does not own the SkyRenderer; the caller manages its lifetime.
- Turbidity defaults to 2.0; call `SkyRenderer::SetTurbidity()` to adjust at runtime.

---

## Skills and instructions followed

- `src/CLAUDE.md`: one class per `.h/.cpp`, Google C++ style, include root `src/`
- `src/environment/CLAUDE.md`: `abstract` + `core` only dependencies (no OpenGL headers)
- `data/shaders/glsl/CLAUDE.md`: `_vs.glsl` / `_ps.glsl` naming, `#include` for uniforms/layouts
- Singleton pattern consistent with `TerrainRenderer`, `FoliageRenderer`
- `cppcheck-suppress unusedStructMember` on all pointer/float fields used only in `.cpp`
