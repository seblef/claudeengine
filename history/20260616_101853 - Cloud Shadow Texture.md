# Cloud Shadow Texture — #552

## Summary

Implemented soft, semi-transparent cloud shadows on ground geometry using a
world-space shadow texture rendered from the cloud density FBM each frame.

## Changes

### New files

| File | Purpose |
|------|---------|
| `data/shaders/glsl/clouds/cloud_density.glsl` | Shared FBM cloud density functions; extracted from `clouds_ps.glsl` |
| `data/shaders/glsl/clouds/cloud_shadow_vs.glsl` | Fullscreen-quad vertex shader for the shadow bake pass |
| `data/shaders/glsl/clouds/cloud_shadow_ps.glsl` | Fragment shader that writes cloud density top-down into the shadow RT |
| `src/environment/CloudShadowRenderer.h` | Singleton renderer: owns the 1024×1024 R16F shadow RT and bake shader |
| `src/environment/CloudShadowRenderer.cpp` | Build / Render / Reset implementation |

### Modified files

| File | Change |
|------|--------|
| `data/shaders/glsl/clouds/clouds_ps.glsl` | Replaced inline FBM helpers with `#include <clouds/cloud_density.glsl>` |
| `data/shaders/glsl/lighting/global_light_ps.glsl` | Added cloud shadow sampler (binding 13) and projection math; attenuation applied to direct sun term only |
| `src/renderer/LightRenderer.h / .cpp` | Added `SetCloudShadow()` + `cloud_shadow_rt_` members; `RenderGlobalLights()` now binds the texture and uploads uniforms |
| `src/renderer/Renderer.h / .cpp` | Added `SetCloudShadowRenderer()`, `cloud_shadow_renderer_` member; step 1b (shadow bake) wired before lighting pass |
| `src/app/main.cpp` | Creates and tears down `CloudShadowRenderer` when `env.cloud_enabled` |
| `src/editor/EnvironmentEditorPanel.cpp` | `EnableCloud` / `DisableCloud` manage `CloudShadowRenderer` lifecycle |
| `src/environment/CMakeLists.txt` | Added `CloudShadowRenderer.cpp` |

## Key decisions

### Shared cloud density include

The FBM, hash, value-noise helpers and the `CloudDensity(world_xz)` wrapper
were extracted into `cloud_density.glsl` so both `clouds_ps.glsl` (visual pass)
and `cloud_shadow_ps.glsl` (shadow bake) produce identical density fields.

### R16F render target (not R8F)

R8F is not a standard OpenGL internal format; `R16F` (already in the
`TextureFormat` enum) provides sufficient precision for shadow density.

### Shadow UV projection formula

```glsl
// direction = incident ray from light → surface  (direction.y < 0 when sun is above)
vec2 cloud_xz  = world_pos.xz + direction.xz / direction.y * kCloudShadowAltitude;
vec2 shadow_uv = (cloud_xz - eye_pos.xz) / cloud_shadow_coverage * 0.5 + 0.5;
```

Derived by tracing the shadow ray from `world_pos` toward the sun
(`L = -direction`) until it hits the cloud plane at `eye_pos.y + 800 m`.
`direction.xz / direction.y` is positive when the sun is above horizon (both
numerator and denominator are negative), placing `cloud_xz` correctly on the
sun-side of the ground point.

### Cloud shadow applied to sun only, not ambient

The issue specified "lerp between full sun and ambient-only term in cloud
shadow". The previous CSM shadow code multiplied the entire output (including
ambient) by the shadow factor. This implementation preserves that for CSM but
adds the cloud shadow cleanly:

```glsl
float sun_factor = 1.0 - shadow_factor * cast_shadow;
vec3  direct     = (diff + spec) * sun_factor * (1.0 - cloud_shadow_factor);
out_color = vec4(direct + ambient * albedo, 1.0);
```

### Sampler binding 13

OmniLight cube shadow maps also use sampler 13 in `omni_light_ps.glsl`. There
is no conflict because GlobalLights are sorted first and fully drawn before any
OmniLight draw overwrites slot 13.  `RenderGlobalLights()` unbinds the cloud
shadow texture before returning.

### Coverage radius default: 4 km

1 texel ≈ (2 × 4000) / 1024 ≈ 7.8 m — fine-grained enough for clouds at 800 m
altitude at typical sun angles. Tunable via `CloudShadowRenderer::SetCoverageRadius()`.

### Shadow intensity default: 0.6

Maximum 60% darkening of the direct sun term, leaving 100% ambient + 40% sun
in the deepest cloud shadow.  Tunable via `CloudShadowRenderer::SetShadowIntensity()`.

## Output / what to keep in mind

- The cloud shadow texture is re-baked every frame (no reprojection). The issue
  mentions reprojection as an optional follow-up optimisation.
- Blur / Gaussian soft-penumbra is **not** implemented; the FBM density field
  already has smooth edges from the `smoothstep` coverage ramp.
- The editor does not yet expose `coverage_radius` and `shadow_intensity` sliders.
  These can be added to `EnvironmentEditorPanel::RenderCloudSection()` as a follow-up.

## Skills and files consulted

- `data/shaders/glsl/CLAUDE.md`
- `src/CLAUDE.md`
- `src/environment/CLAUDE.md`
- `src/editor/CLAUDE.md`
- `CLAUDE.md` (root)
