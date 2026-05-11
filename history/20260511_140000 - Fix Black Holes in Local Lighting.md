# Fix: Black Holes in Local Light Shading

## Summary

Fixed a rendering bug where local lights (OmniLight, CircleSpotLight, RectangleSpotLight) produced
black holes on lit objects in the deferred shading pipeline.

## Root Cause

`light_pass_vs.glsl` computed `v_screen_uv` by dividing clip-space XY by W in the **vertex** shader:

```glsl
v_screen_uv = (clip.xy / clip.w) * 0.5 + 0.5;
```

GLSL's default perspective-correct interpolation then applies a second implicit perspective
correction across the triangle:

```
interpolated = (sum_i(v_i / w_i * bary_i)) / (sum_i(1/w_i * bary_i))
```

Because `v_i` is already `clip.xy_i / clip.w_i`, this collapses to `clip.xy / clip.w²` — a
quadratic distortion of the screen position rather than the correct linear one. For the fullscreen
quad used by GlobalLight (`w=1` everywhere), the distortion is zero so the global light rendered
correctly. For the light volume meshes (sphere, cone, pyramid) where `w` varies, the G-buffer was
sampled at incorrect screen coordinates, causing albedo/normal/depth to be read from wrong pixels.
The incorrect depth produced a wrong world-position reconstruction, which in turn made the
per-pixel distance from the light volume wildly inaccurate — most pixels received zero or garbage
attenuation, appearing black.

## Fix

Removed `v_screen_uv` from both vertex shader outputs (`light_pass_vs.glsl` and
`global_light_vs.glsl`) and replaced all uses in the four fragment shaders with:

```glsl
vec2 v_screen_uv = gl_FragCoord.xy * inv_screen_size;
```

`gl_FragCoord.xy` is the window-space position of the current fragment (already rasterized
correctly by the GPU). Multiplying by `inv_screen_size` (= 1/viewport_size, from the SceneInfos
UBO already included by all lighting shaders) maps it exactly to [0, 1] UVs. No interpolation is
involved — the value is computed per-fragment, so there is no opportunity for a perspective
correction error.

## Files Changed

| File | Change |
|---|---|
| `data/shaders/glsl/lighting/light_pass_vs.glsl` | Remove `out vec2 v_screen_uv` and its assignment |
| `data/shaders/glsl/lighting/global_light_vs.glsl` | Same removal for consistency |
| `data/shaders/glsl/lighting/global_light_ps.glsl` | Replace `in vec2 v_screen_uv` with `gl_FragCoord.xy * inv_screen_size` |
| `data/shaders/glsl/lighting/omni_light_ps.glsl` | Same |
| `data/shaders/glsl/lighting/circle_spot_ps.glsl` | Same |
| `data/shaders/glsl/lighting/rect_spot_ps.glsl` | Same |

## Output to Keep in Mind

- **Always derive screen-space sampling UVs from `gl_FragCoord` in deferred fragment shaders**, not
  from varyings computed in the vertex shader. Pre-dividing by `w` in the VS and relying on
  perspective-correct interpolation produces double-correction artifacts for non-flat meshes.
- The `inv_screen_size` field in `SceneInfos` is the right tool for this: `gl_FragCoord.xy * inv_screen_size`.
- This pattern applies to any future deferred-style pass that renders a non-fullscreen volume mesh
  (e.g., area lights, volumetric fog bounds, decal volumes).
