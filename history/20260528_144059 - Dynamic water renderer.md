# Dynamic Water Renderer (#323)

## Overview

Added a `WaterRenderer` singleton to the `environment` module that renders a
dynamic water surface at a configurable world-space height.  Waves are animated
by the existing `WindInfos` constant buffer (slot 7).  The fragment shader uses
Schlick Fresnel blending between a deep-water colour and the sky zenith colour,
plus a Blinn-Phong specular highlight from the current sun direction.

## New files

| File | Role |
|---|---|
| `src/environment/WaterInfos.h` | GPU constant buffer struct (slot 9, 48 bytes / 3 float4s) |
| `src/environment/WaterRenderer.h` | Singleton class declaration |
| `src/environment/WaterRenderer.cpp` | Build, render, and reset implementation |
| `data/shaders/glsl/uniforms/water_infos.glsl` | GLSL uniform block (binding 9) |
| `data/shaders/glsl/water/water_vs.glsl` | Vertex shader: wave displacement + normal |
| `data/shaders/glsl/water/water_ps.glsl` | Fragment shader: Fresnel + specular → G-buffer |

## Modified files

| File | Change |
|---|---|
| `src/environment/CMakeLists.txt` | Added `WaterRenderer.cpp` |
| `src/renderer/Renderer.h` | Added `SetWaterRenderer()`, `SetWaterSkyWorldTime()`, `UpdateWaterRenderer()` declaration |
| `src/renderer/Renderer.cpp` | Included `WaterRenderer.h`; geometry pass calls water renderer; added `UpdateWaterRenderer()` |

## Design decisions

### WaterInfos struct layout

`Vec3f` is `alignas(16)` and thus 16 bytes in C++, mirroring how GLSL std140
also pads `vec3` to 16 bytes.  However, interleaving `Vec3f` with floats wastes
12 bytes of padding between them (the next `Vec3f` must start at a 16-byte
boundary).  To keep the struct compact (48 bytes = 3 float4s), `WaterInfos` uses
twelve raw `float` members arranged as three float4s, and the GLSL uniform block
declares them as `vec4` types:

```
water_params      vec4  → .rgb = water_color, .a = water_level
sky_zenith_color  vec4  → .rgb = sky zenith,  .a = unused
sun_direction     vec4  → .xyz = sun dir,     .w = unused
```

This avoids the 12-byte padding waste and is verified with
`static_assert(sizeof(WaterInfos) == 48)`.

### Constant buffer slot 9

Existing slots 1–3, 5, 7, and 8 are occupied.  Slot 9 was the next free slot.

### Wave model

Four sine waves are layered in the vertex shader; their directions are rotations
of the wind direction by 0°, +30°, −20°, and +60°.  Frequencies (0.05–0.12 1/m)
and phase speeds (0.5–1.2 m/s) differ per wave to break up the regular pattern.
Amplitudes are capped at 2 m and scale with `wind_strength` so that calm weather
produces a flat surface.  The wave normal is derived analytically from the sum of
sine derivatives, avoiding texture lookups.

### Sky zenith colour approximation

`Renderer::UpdateWaterRenderer()` derives a simple day/night interpolation from
`water_sky_world_time_` (same value as `sky_world_time_`).  This matches the
visual output of the Preetham sky qualitatively without needing to reach into
`SkyRenderer`'s internals.

### Render order

Water is rendered **in the geometry pass** after terrain and foliage, so it
participates in the deferred lighting pass and receives shadow projection from the
existing `ShadowRenderer`.  Water is NOT rendered in the shadow depth pass — it
receives shadows but does not cast them.

## Integration usage

```cpp
// Build
WaterRenderer& water = *new environment::WaterRenderer();
water.Build(video, env_desc.water_level);
Renderer::Instance().SetWaterRenderer(&water);
Renderer::Instance().SetWaterSkyWorldTime(world_time.GetTimeOfDay());

// Each frame (in game loop, before Renderer::Update):
Renderer::Instance().SetWaterSkyWorldTime(world_time.GetTimeOfDay());

// Teardown
water.Reset();
WaterRenderer::Shutdown();
```

## Skills / CLAUDE.md sections used

- `src/CLAUDE.md`: one class per .h/.cpp, Google C++ style, project-relative includes
- `src/environment/CLAUDE.md`: singleton pattern, no dep on `renderer`, header-only data structs
- `data/shaders/glsl/CLAUDE.md`: vertex+pixel pair naming, `#include` for uniforms/layouts

## Notes for next features

- Water grid is a fixed 128×128 quad mesh centred at the origin (1 280 m × 1 280 m
  at 10 m/cell).  A future enhancement could track the camera XZ position to
  create a truly infinite horizon.
- The Fresnel Schlick approximation uses the full 5th-power form; a cheaper
  `1 - n_dot_v` lerp could be used if perf becomes an issue.
- Rivers or lakes at different heights are not supported; all water is at a single
  global `water_level`.
