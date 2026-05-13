# CSM GlobalLight (Issue #149)

## Summary

Implemented Cascaded Shadow Maps (CSM) for the `GlobalLight` (directional sun). A single orthographic shadow map has insufficient resolution for large scenes; CSM splits the camera frustum into 4 depth slices and renders a separate shadow map per slice, giving high resolution near the camera and lower resolution far away.

## Changes

### `src/core/Mat4f.h`
Added `OrthoOffCenterRH(left, right, bottom, top, z_near, z_far)` — an asymmetric right-handed orthographic projection mapping an off-center AABB to the NDC cube. Required for tight-fit cascade projections.

### `src/renderer/CSMInfos.h` (new)
GPU constant buffer layout for slot 5 (272 bytes, 17 float4s):
- `cascade_vp[4]` — 4 × mat4 light-space VP matrices (offsets 0–255)
- `split_x/y/z/w` — view-space far depths for cascades 0–3 (offset 256)

### `data/shaders/glsl/uniforms/csm_infos.glsl` (new)
GLSL uniform block matching `CSMInfos` with `layout(std140, row_major, binding=5)`.

### `src/renderer/GlobalLight.h/.cpp`
Added `ComputeCascadeMatrices(const core::Camera& camera, CSMInfos& out) const`:
- Uses the **practical split scheme** (λ=0.5 blend of log and uniform splits) to compute 4 cascade boundaries
- For each cascade: computes 8 view-space frustum corners, transforms to world, finds centroid and bounding sphere
- Places light eye at `centroid - direction_ * bounding_sphere_radius` (sun above scene)
- Builds `LookAtRH` view matrix, transforms corners to light-view, computes tight AABB
- Generates `OrthoOffCenterRH` projection with z_far extended by bounding sphere radius for casters behind the frustum
- Stores `ortho * light_view` as cascade VP

### `src/renderer/ShadowRenderer.h/.cpp`
- Added `has_csm_`, `csm_infos_`, `cascade_maps_[4]` members
- Added `RenderCascades()` private method: lazily allocates 4 ShadowMap objects at the GlobalLight's resolution, calls `ComputeCascadeMatrices`, renders depth pass per cascade
- Added `HasCSM()`, `GetCSMInfos()`, `GetCascadeMap(int)` accessors
- In `RenderShadowMaps()`: detects GlobalLight with `cast_shadow=true` and calls `RenderCascades()` before the spot-light pool loop

### `src/renderer/Renderer.h/.cpp`
- Added `csm_infos_cb_` (slot 5, 17 float4s) created in constructor
- Bound in `Update()` at frame start alongside other CBs
- Filled with CSM data after shadow rendering when `HasCSM()` is true

### `src/renderer/LightRenderer.cpp`
- In `RenderGlobalLights()`: binds cascade shadow maps to sampler slots 9–12 before drawing

### `data/shaders/glsl/lighting/global_light_ps.glsl`
- Added `#include <uniforms/csm_infos.glsl>`
- Added 4 `sampler2DShadow` cascade samplers at bindings 9–12
- Shadow computation: selects cascade by view-space depth (compared to `split_distances.x/y/z`), projects world_pos through `cascade_vp[cascade]`, perspective-divides, remaps to [0,1], applies `shadow_bias`, samples via `SampleCascadeShadow()` helper (switch-based to avoid dynamic sampler array indexing)
- Final output: `(diff + spec + ambient * albedo) * (1.0 - shadow_factor * cast_shadow)`

## Decisions

**Practical split scheme (λ=0.5)**: Industry standard — balances logarithmic (sharp near transitions) and uniform (even coverage). λ=0.5 is a well-regarded default.

**Bounding sphere for eye placement**: Placing the light eye at `centroid - direction_ * bounding_sphere_radius` ensures all frustum corners are always in front of the shadow camera, avoiding artifacts from z-clipping. Extending z_far by the same amount captures shadow casters outside the view frustum.

**GlobalLight outside the pool**: There is only one directional light and it always shadows when enabled, so pool overhead would add complexity without benefit.

**Switch-based cascade sampler selection** (not dynamic array indexing): Avoids dynamic non-uniform sampler indexing which has undefined behavior in GLSL 4.5 without extensions, even though most drivers handle it in practice.

**`GL_LEQUAL` shadow comparison**: Already configured in `GLRenderTarget` for `kDepth32F` format, enabling hardware PCF via `sampler2DShadow`. Returns 1.0 (lit) when compare_z ≤ depth.

## Notes for next features

- The `view * proj` convention in `CircleSpotLight::ComputeShadowVP()` is inconsistent with Camera.cpp's `proj * view` convention. GlobalLight CSM uses `proj * view` (correct). This should be fixed when spot-light shadow sampling is implemented in a future issue.
- The cascade shadow map resolution is lazy-reallocated when `GetShadowResolution()` changes; this triggers a GPU allocation per frame if resolution is changed frequently (not expected in practice).
- `split_distances.w` (far depth of cascade 3) is stored in CSMInfos but not used in the shader's cascade selection — it is available for potential future use (e.g., fade-out beyond cascade 3).

## Skills used
- `impl-issue`

## CLAUDE.md instructions followed
- One class per `.h/.cpp` pair: `GlobalLight` retains its file pair; `CSMInfos` is header-only (POD struct)
- Google C++ style guide: all cpplint checks pass
- Conventional commit message format
- History file written per contribution requirement
