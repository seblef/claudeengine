# Spot Light Shadow Maps (CircleSpotLight and RectangleSpotLight)

**Date:** 2026-05-13
**Branch:** feat/spot-light-shadows
**Issue:** #150

---

## Summary

Implemented perspective shadow maps for `CircleSpotLight` and `RectangleSpotLight`. Each spot light that has `cast_shadow == true` gets one depth-only shadow map from the `ShadowMapPool`, rendered during the shadow pass and sampled in the lighting fragment shaders via `sampler2DShadow`.

---

## Changes

### `src/renderer/CircleSpotLight.h` / `.cpp`

Added `GetLightSpaceMatrix() const -> core::Mat4f`:

- **View**: `LookAtRH(position, position + direction, up)` where `up = {1,0,0}` when `|direction.y| > 0.9` (gimbal lock avoidance).
- **Projection**: symmetric perspective `PerspectiveRH(2 * outer_angle, aspect=1.0, near=0.1, far=range)`.
- Returns `proj * view` (column-vector convention).

`ComputeShadowVP()` now delegates to `GetLightSpaceMatrix()` (previously the implementation was missing entirely; only a stub returning nullopt existed).

### `src/renderer/RectangleSpotLight.h` / `.cpp`

Added `GetLightSpaceMatrix() const -> core::Mat4f`:

- **View**: same `LookAtRH` construction.
- **Projection**: `PerspectiveRH(2 * v_angle, aspect=tan(h_angle)/tan(v_angle), near=0.1, far=range)`. The aspect ratio maps the rectangular frustum's horizontal extent correctly.
- Returns `proj * view`.

`ComputeShadowVP()` delegates to `GetLightSpaceMatrix()`.

### `src/renderer/LightRenderer.cpp`

In `RenderLocalLights()`, after the shader/geometry state switch and before sub-pass A, added a conditional bind for spot lights:

```cpp
if (light->GetType() == LightType::kCircleSpot ||
    light->GetType() == LightType::kRectSpot) {
  const ShadowMap* smap = ShadowRenderer::Instance().GetShadowMap(light);
  if (smap) smap->GetDepthRT()->BindAsSampler(9);
}
```

Sampler slot 9 is reserved for the per-spot-light shadow depth texture. The omni light shader is unaffected (different type, no binding).

### `data/shaders/glsl/lighting/circle_spot_ps.glsl`

- Added `layout(binding = 9) uniform sampler2DShadow u_shadow_map;`.
- Shadow computation after Blinn-Phong:
  ```glsl
  float shadow = 0.0;
  if (cast_shadow > 0.5) {
      vec4 shadow_coord = light_vp * vec4(world_pos, 1.0);
      shadow_coord.xyz /= shadow_coord.w;
      shadow_coord.xyz  = shadow_coord.xyz * 0.5 + 0.5;
      shadow_coord.z   -= shadow_bias;
      shadow = 1.0 - texture(u_shadow_map, shadow_coord.xyz);
  }
  out_color = vec4((diff + spec) * atten * falloff * (1.0 - shadow), 1.0);
  ```

### `data/shaders/glsl/lighting/rect_spot_ps.glsl`

Same pattern as `circle_spot_ps.glsl`. Shadow factor multiplied into `h_fall * v_fall`.

---

## Decisions and Rationale

### `proj * view` convention

The project uses column-vector convention throughout (`v' = M * v` in math, which means the view-projection is `proj * view` applied right-to-left). The previous `ComputeShadowVP()` implementations were placeholders; the new `GetLightSpaceMatrix()` methods return `proj * view` consistently.

### Aspect ratio for RectangleSpotLight

`aspect = tan(h_angle) / tan(v_angle)` maps the frustum's horizontal half-width to vertical half-height ratio. Using `fov_y = 2 * v_angle` with this aspect gives correct `left/right = ±tan(h_angle) * near` and `bottom/top = ±tan(v_angle) * near`.

### Sampler slot 9 for spot lights vs CSM

CSM cascades use slots 9–12 in the global light pass. Spot lights run in a separate `RenderLocalLights()` sub-pass; the global light pass completes first. Sampler slot 9 is reused for each spot light (only one is active at a time in the stencil-based draw).

### Shadow bias via `shadow_bias` uniform field

`shadow_bias` is already provided by `LightInfos` (established in issue #148). It is subtracted from `shadow_coord.z` before the hardware PCF comparison, which avoids acne without requiring per-light CPU-side tweaking.

### `cast_shadow > 0.5` float branch

Float-as-bool encoding from issue #148: `cast_shadow` is 0.0 or 1.0, tested with `> 0.5` to avoid a uniform branch on the boolean. When `cast_shadow == 0.0`, `shadow` stays 0.0 and the multiplication by `(1.0 - shadow)` is a no-op.

---

## Key Points for Future Features

- Sampler slots 9–12 are shared between CSM (global light pass) and spot light shadows (local light pass). They do not conflict because the passes are sequential.
- Omni lights (point lights) are not handled here — they require a cube shadow map (`samplerCubeShadow`), which is a separate feature.
- `ShadowMapPool` allocates 2D shadow maps for spot lights. Pool exhaustion silently disables shadows for that light (`cast_shadow = 0.0`).
- `GetLightSpaceMatrix()` is public and may be useful for debug visualization (e.g., drawing the shadow frustum in the editor).

---

## Skills / Instructions Followed

- `impl-issue` skill (feat/ branch, conventional commit, PR to dev, history file)
- `CLAUDE.md`: one class per file, Google C++ style, `cppcheck-suppress unusedStructMember` on private members
- `data/shaders/glsl/CLAUDE.md`: `#include` for uniforms, documentation comments for equations
- Column-vector math convention (`proj * view`)
- cpplint passed with no warnings
