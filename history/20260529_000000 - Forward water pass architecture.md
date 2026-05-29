# Forward Water Pass Architecture

**Date**: 2026-05-29  
**Issue**: #352  
**Branch**: `feat/renderer-water-forward-pass-352`

---

## Summary

Moved water rendering from the G-buffer geometry pass to a forward pass that runs
after the deferred lighting + emissive passes. This is the architectural prerequisite
for refraction (scene-colour sampling), depth-based shoreline effects (foam, SSR), and
alpha blending at the shoreline.

---

## Changes

### `src/environment/WaterInfos.h`
Expanded the constant buffer from 3 to 6 float4s (48 → 96 bytes).  The `static_assert`
was updated accordingly.  New fields (with their defaults):

| float4 | fields |
|--------|--------|
| 0 | water_color (rgb) + water_level (a) — unchanged |
| 1 | sky_zenith_color (rgb) + **roughness** (a = 0.05) |
| 2 | sun_direction (xyz) + **sun_intensity** (w = 20.0) |
| 3 | **refraction_strength** (0.03), **absorption_scale** (0.20), **foam_height_thresh** (0.60), **foam_shoreline_depth** (2.0) |
| 4 | **foam_steepness_thresh** (0.70), **foam_speed** (1.5), **normal_scale1** (0.03), **normal_scale2** (0.07) |
| 5 | **normal_scroll_speed1** (0.30), **normal_scroll_speed2** (0.20), pad, pad |

### `src/environment/WaterRenderer.h` / `.cpp`
- Removed `water_cb_` (constant buffer ownership moved to `Renderer`).
- Changed `Render()` signature to `Render(camera, scene_color*, depth*)`.
- `Render()` now binds `scene_color` at sampler slot 2, `depth` at sampler slot 3,
  sets LEQUAL depth test / depth write off, draws the grid, then unbinds samplers 2 & 3.
- Exposed getters (`GetWaterLevel()`, `GetSkyZenithR/G/B()`, `GetSunDirection()`) so
  `Renderer::UpdateWaterRenderer()` can build the WaterInfos CB without a back-reference.

### `src/abstract/VideoDevice.h`
Added two pure virtual methods:
- `CopyRenderTarget(src, dst)` — pixel-exact copy via `glCopyImageSubData`.
- `CreateTileableTexture(w, h, data)` → `unique_ptr<RawTexture>` — RGBA8 with
  `GL_REPEAT` + `GL_LINEAR_MIPMAP_LINEAR` + auto-generated mipmaps.

### `src/gldevices/GLVideoDevice.h` / `.cpp`
Implemented both new abstract methods:
- `CopyRenderTarget` uses `glCopyImageSubData` (OpenGL 4.3+) with texture IDs
  extracted via `GetNativeHandle()`.  No framebuffer rebinding needed.
- `CreateTileableTexture` returns a new `GLTileableTexture`.

### `src/gldevices/GLTileableTexture.h` / `.cpp` *(new files)*
`GLTileableTexture` implements `abstract::RawTexture`:
- `GL_RGBA8` internal format.
- `GL_REPEAT` wrapping on both axes.
- `GL_LINEAR_MIPMAP_LINEAR` / `GL_LINEAR` filtering.
- Mipmaps auto-generated at construction and after every `UpdateRegion()` call.

### `src/gldevices/CMakeLists.txt`
Added `GLTileableTexture.cpp` to the `gldevices` static library.

### `src/renderer/Renderer.h` / `.cpp`
- `water_infos_cb_` (slot 9, 6 float4s) created at Renderer level and bound
  globally at the start of `Update()` — terrain shaders can read `water_level`
  during the geometry pass.
- `water_scene_color_rt_` (RGBA16F) and `water_depth_copy_rt_` (DEPTH24STENCIL8)
  allocated at construction; recreated in `OnResize()` and `ResizeTargets()`.
- `UpdateWaterRenderer()` now also fills `water_infos_cb_` with current sky-derived
  sun direction + sky zenith colour + water_renderer_ state.  It is called once per
  frame **before** the geometry pass so the CB is populated when terrain shaders run.
- **Geometry pass**: removed `water_renderer_->Render()`.
- **Forward water pass** (step 4b, after emissive, before composite):
  1. `CopyRenderTarget(hdr_rt, water_scene_color_rt_)` — snapshot scene colour.
  2. `CopyRenderTarget(gbuffer_depth, water_depth_copy_rt_)` — snapshot depth.
  3. Bind emissive FBO; set LEQUAL depth test / depth write off / SrcAlpha blend.
  4. `water_renderer_->Render(camera, water_scene_color_rt_, water_depth_copy_rt_)`.
  5. Restore blend / depth state; unbind FBO.

### `data/shaders/glsl/uniforms/water_infos.glsl`
Updated the `WaterInfosBlock` uniform block to 6 float4s matching the new C++ layout:
`water_params`, `sky_zenith_color`, `sun_params`, `refraction_params`,
`foam_params`, `scroll_params`.

### `data/shaders/glsl/water/water_ps.glsl`
Rewrote for the forward pass:
- **Removed** 3-MRT G-buffer outputs; replaced with `layout(location=0) out vec4 out_color`.
- Added `uniform sampler2D scene_color_tex` (slot 2) and `uniform sampler2D depth_tex` (slot 3).
- `LinearizeDepth()` converts raw depth buffer values using `z_near` / `z_far` from scene_infos.
- Water column depth computed as `max(linearScene - linearWater, 0)`.
- Shoreline foam computed from depth vs `foam_shoreline_depth`.
- Refraction: screen UV distorted by `N.xz * refraction_strength`, then absorption-tinted.
- Surface colour: Fresnel blend of absorbed refraction and sky zenith.
- Specular: Blinn-Phong with `shininess = 2/roughness² - 2`, scaled by `sun_intensity`.
- Alpha formula (from issue spec):
  ```glsl
  out_alpha = smoothstep(0.0, 1.5, water_depth);
  out_alpha = max(out_alpha, foam_amount);
  out_alpha = clamp(out_alpha + fresnel * (1.0 - out_alpha), 0.0, 1.0);
  ```

---

## Decisions and rationale

**Why promote water_cb_ to Renderer?**  
The issue explicitly requires terrain shaders to read `water_level` during the geometry
pass (needed by upcoming caustics/wet-terrain issues).  A Renderer-owned CB bound before
the geometry pass is the same pattern used by `wind_infos_cb_` and `scene_infos_cb_`.

**Why `glCopyImageSubData` for CopyRenderTarget?**  
Available since OpenGL 4.3 (well within our 4.6 requirement).  Copies pixels entirely
on the GPU without binding an FBO, making it safe to call mid-frame without disturbing
the currently bound framebuffer.

**Why allocate water_depth_copy_rt_ as DEPTH24STENCIL8 (not DEPTH32F)?**  
The G-buffer depth is DEPTH24STENCIL8 — `glCopyImageSubData` requires identical internal
formats, so the copy target must match.

**Why SrcAlpha / OneMinusSrcAlpha blending?**  
Water alpha encodes both depth-based opacity (deep water fully opaque) and shoreline
fade-in. Standard alpha blending correctly composites the water RGBA onto the HDR
scene without re-sampling the scene colour from the default framebuffer.

---

## Output — important for next features

- The water CB (slot 9) is now globally bound before the geometry pass. Terrain shaders
  can `#include <uniforms/water_infos.glsl>` and read `water_params.a` (water_level) with
  no extra setup.
- `water_scene_color_rt_` and `water_depth_copy_rt_` are available as snapshots after
  the emissive pass. Future PRs (SSR, volumetric caustics) can read these directly via
  additional sampler slots without re-copying.
- `GLTileableTexture` is ready to serve water normal-map and foam textures: create with
  `video->CreateTileableTexture(w, h, data)` and bind at any sampler slot in the water
  shader.
- The foam params (float4 3–4) and normal scroll params (float4 5) are wired into the CB
  but the shader currently only uses `foam_shoreline_depth`. Future PRs can plug in
  steepness-based foam, dual-layer normal maps, and speed-based foam animation.

---

## Skills and instructions followed

- `impl-issue` skill: issue requirements directly implemented.
- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; include root `src/`; new module needs `CMakeLists.txt` entry.
- `src/environment/CLAUDE.md`: environment module must not depend on renderer.
- `data/shaders/glsl/CLAUDE.md`: vertex and pixel programs in separate files with matching name prefix.
- Root `CLAUDE.md`: optimized but readable code; no unnecessary abstractions; no unreachable error handling.
