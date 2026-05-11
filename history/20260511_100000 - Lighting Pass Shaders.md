# Lighting Pass Shaders

**Issue**: #84
**Branch**: feat/lighting-pass-shaders

## Overview

Nine GLSL shader files implementing the deferred lighting pass — one vertex shader per light type (sharing a common body for local lights) and one fragment shader per light type. Together they complete the GPU side of the `LightRenderer` introduced in issue #83.

## Files Added

```
data/shaders/glsl/lighting/
  light_pass_vs.glsl      — shared VS body (partial include, no #version)
  global_light_vs.glsl    — standalone NDC passthrough VS for fullscreen quad
  omni_light_vs.glsl      — #version + #include <lighting/light_pass_vs.glsl>
  circle_spot_vs.glsl     — same pattern
  rect_spot_vs.glsl       — same pattern
  global_light_ps.glsl    — directional Blinn-Phong + ambient, no attenuation
  omni_light_ps.glsl      — point light, inverse-square attenuation
  circle_spot_ps.glsl     — cone spot, attenuation × inner/outer smoothstep
  rect_spot_ps.glsl       — rectangular spot, attenuation × H-axis × V-axis falloff
```

## Key Decisions

### One VS body, four entry points

`light_pass_vs.glsl` is a plain include fragment (no `#version`, no header guard) containing the shared vertex shader body for the three local-light types. Each per-type `_vs.glsl` is a one-liner: `#version 460 core` followed by `#include <lighting/light_pass_vs.glsl>`. This avoids duplication while remaining compatible with the engine's shader loader (which loads `name_vs.glsl` + `name_ps.glsl` as a pair).

`global_light_vs.glsl` is standalone because the GlobalLight uses a fullscreen NDC quad — vertices are already in clip space and no world/VP transform should be applied.

### G-buffer sampler bindings (slots 5–8)

Slots 0–2 are used by the geometry pass (diffuse, normal, specular textures). Slots 3–4 are left for future use. Lighting pass binds at 5–8 so both passes can coexist in the same texture unit table without conflicts:

| Slot | Sampler |
|---|---|
| 5 | `u_albedo` (RGBA8) |
| 6 | `u_normal` (RGBA16F) |
| 7 | `u_specular` (RGBA8) |
| 8 | `u_depth` (DEPTH24_STENCIL8) |

### World position reconstruction

Each fragment shader reconstructs world position from the depth buffer:
```glsl
float raw_depth = texture(u_depth, v_screen_uv).r;
vec4  ndc       = vec4(v_screen_uv * 2.0 - 1.0, raw_depth * 2.0 - 1.0, 1.0);
vec4  world_h   = inv_view_proj * ndc;
vec3  world_pos = world_h.xyz / world_h.w;
```
This avoids a dedicated world-position G-buffer render target, saving 16 bytes/pixel of bandwidth.

### Blinn-Phong

All shaders use the same diffuse + specular equations:
```glsl
vec3 diff = max(dot(N, L), 0.0) * albedo * color * intensity;
vec3 spec = pow(max(dot(N, H), 0.0), shininess) * spec_int * color * intensity;
```
`shininess` is decoded as `texture(u_specular).g * 256.0` (packed in the G-buffer by `gbuffer_ps.glsl`).

### Circle spot angular falloff

```glsl
float cos_angle = dot(-L, normalize(direction));
float falloff   = smoothstep(cos(outer_angle), cos(inner_angle), cos_angle);
```
Since both angles are < π/2, `cos(outer_angle) < cos(inner_angle)`, so `smoothstep` returns 0 at the outer edge and 1 inside the inner cone — correct brightening toward center.

### Rect spot per-axis falloff

The light's local `right`/`up` frame is reconstructed from `direction`, mirroring `AlignZToDir` in `RectangleSpotLight.cpp`:
```glsl
vec3 ref    = abs(direction.y) > 0.99 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 1.0, 0.0);
vec3 right  = normalize(cross(ref, direction));
vec3 up_dir = cross(direction, right);
```

`atan(dot(-L, axis), dot(-L, direction))` is used instead of `asin` for correct angular measurement at large angles. Each axis fades over the outermost 20% of its half-angle:
```glsl
float h_fall = 1.0 - smoothstep(h_angle * 0.8, h_angle, h_angle_px);
float v_fall = 1.0 - smoothstep(v_angle * 0.8, v_angle, v_angle_px);
```

## What's Next

- Issue #87: Wire `LightRenderer` into `Renderer::Update()` to run the full 4-phase pipeline (geometry → lighting → emissive → composite).
- Issue #88: Debug G-buffer view modes (CPU-side bypass).
- Issue #89: Demo scene with all four light types.
