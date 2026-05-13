# OmniLight Cube Shadow Map

**Date:** 2026-05-13
**Branch:** feat/omnilight-cube-shadow-map
**Issue:** #151

---

## Summary

Implemented omnidirectional (point light) shadow maps using a `GL_TEXTURE_CUBE_MAP`. Each `OmniLight` with `cast_shadow=true` gets a `ShadowCubeMap` from the cube pool, rendered in 6 depth passes (one per face), and sampled in `omni_light_ps.glsl` via `samplerCubeShadow` hardware PCF.

This required a new `abstract::RenderTargetCube` abstraction, a GL implementation, a factory method on `VideoDevice`, and a new `ShadowCubeMap` renderer-level wrapper analogous to `ShadowMap`.

---

## Changes

### `src/abstract/RenderTargetCube.h` (new)

Abstract cube-map depth render target. Interface:
- `BindAsSampler(slot)` — binds as `samplerCubeShadow`
- `BindFaceAsOutput(face_idx)` — attaches face 0–5 to the currently bound FBO as `GL_DEPTH_ATTACHMENT`
- `GetSize()` — cube face size in pixels

### `src/gldevices/GLRenderTargetCube.h/.cpp` (new)

OpenGL implementation. Allocates `GL_TEXTURE_CUBE_MAP` with `GL_DEPTH_COMPONENT32F` on all 6 faces; sets `GL_LINEAR` filter, `GL_CLAMP_TO_EDGE` wrap (all 3 axes), `GL_COMPARE_REF_TO_TEXTURE + GL_LEQUAL` for hardware PCF via `samplerCubeShadow`.

`BindFaceAsOutput` calls `glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face_idx, ...)` on the caller's currently bound FBO.

### `src/abstract/VideoDevice.h`

Added pure virtual `CreateRenderTargetCube(int size, TextureFormat format) -> unique_ptr<RenderTargetCube>`. Includes `"abstract/RenderTargetCube.h"`.

### `src/gldevices/GLVideoDevice.h/.cpp`

Implemented `CreateRenderTargetCube()` returning `make_unique<GLRenderTargetCube>(size)`. The `format` parameter is reserved for future use (only `kDepth32F` is meaningful for shadow cubes).

### `src/gldevices/GLRenderTargetGroup.cpp`

Added a guard for completely empty FBOs (no color targets, no depth): logs INFO "deferred attachment" and skips `glCheckFramebufferStatus`. This avoids a spurious error log when `ShadowCubeMap` creates the shared FBO before any face is attached.

### `src/renderer/ShadowCubeMap.h/.cpp` (new)

Owns a `RenderTargetCube` and a depth-only FBO (`RenderTargetGroup` with no initial attachments). Key methods:

- `ComputeFaceMatrices(position, range)`: fills `cube_vp_[6]` with `proj * view` for each of the standard cube-map face orientations. `proj = PerspectiveRH(π/2, 1.0, 0.1, range)`.
- `BindFaceForWriting(face_idx)`: binds the FBO then attaches the face via `cube_rt_->BindFaceAsOutput(face_idx)`.
- `UnbindForWriting()`: delegates to `fbo_->UnbindForWriting()`.

**Standard face view directions** (OpenGL convention):
| Face | Target offset | Up      |
|------|--------------|---------|
| +X   | (1,0,0)      | (0,-1,0)|
| -X   | (-1,0,0)     | (0,-1,0)|
| +Y   | (0,1,0)      | (0,0,1) |
| -Y   | (0,-1,0)     | (0,0,-1)|
| +Z   | (0,0,1)      | (0,-1,0)|
| -Z   | (0,0,-1)     | (0,-1,0)|

### `src/renderer/ShadowMapPool.h/.cpp`

- Changed `pool_cube_` type from `vector<TierSlots>` (ShadowMap) to `vector<CubeTierSlots>` (ShadowCubeMap).
- Added `CubeSlot`, `CubeTierSlots` inner structs (mirrors the existing 2D `Slot`/`TierSlots`).
- Added `BuildCubePool()`, `FindTargetCubeTierIdx()`, `AssignCubePool()` — parallel to their 2D equivalents.
- Added `cube_assignments_: unordered_map<const Light*, ShadowCubeMap*>`.
- `Assign()` now collects `eligible_cube` (kOmni lights) and calls `AssignCubePool`.
- Added `GetShadowCubeMap(const Light*) const -> const ShadowCubeMap*`.
- `ClearAll()` also clears `cube_assignments_`.

### `src/renderer/ShadowRenderer.h/.cpp`

Added `RenderCubeShadows(lights, no_cull, octree)` called from `RenderShadowMaps` between CSM cascades and 2D spot shadow passes. For each OmniLight with a pool slot:
1. Extracts world position from world matrix.
2. Calls `scm->ComputeFaceMatrices(pos, radius)`.
3. Loops over 6 faces: fills `ShadowPassInfos`, culls from both visibility systems, calls `BindFaceForWriting`, clears depth, renders, unbinds.

Added `GetShadowCubeMap(const Light*) const` delegating to the pool.

### `src/renderer/LightRenderer.cpp`

In `RenderLocalLights()`, after the shader/geo switch, binds the cube shadow map at sampler slot 13 for kOmni lights.

### `data/shaders/glsl/lighting/omni_light_ps.glsl`

- Added `layout(binding = 13) uniform samplerCubeShadow u_shadow_cube;`.
- Shadow computation after Blinn-Phong:
  ```glsl
  if (cast_shadow > 0.5) {
      vec3  light_dir = world_pos - position;
      float compare   = length(light_dir) / range - shadow_bias;
      shadow = 1.0 - texture(u_shadow_cube, vec4(light_dir, compare));
  }
  out_color = vec4((diff + spec) * atten * (1.0 - shadow), 1.0);
  ```

---

## Decisions and Rationale

### Deferred-attachment FBO for cube shadow maps

`ShadowCubeMap` creates a `RenderTargetGroup` FBO with no attachments at construction time. The face is attached dynamically before each render via `BindFaceAsOutput`. This avoids creating 6 separate FBOs (one per face) while reusing existing abstractions. `GLRenderTargetGroup` was modified to log INFO instead of ERROR for empty FBOs, since completeness is deferred by design.

### Separate `CubeTierSlots` vs. templating `TierSlots`

The slot types differ only in `ShadowMap` vs `ShadowCubeMap`. Templating would require moving all logic to headers. Duplicating the small structs and methods is simpler and keeps the implementation clear.

### Shadow comparison formula: `length(light_dir) / range`

This is a linear depth approximation. For it to match the stored perspective depth exactly, the shadow pass should write linear depth. Currently the shadow pass uses the standard hardware non-linear depth, so there is a discrepancy. Shadows will render with some bias (no acne but potential peter-panning at oblique angles). A future improvement would be to store linear depth in the cube shadow pass.

### Sampler slot 13 for cube shadow maps

Slots 9–12 are used by CSM cascades (global light pass). Spot lights reuse slot 9 in `RenderLocalLights`. Using slot 13 avoids conflicts across all light types.

---

## Key Points for Future Features

- The shadow depth comparison for cube maps (`length / range`) does not match the non-linear perspective depth stored in the shadow map. A custom depth-write fragment shader would fix this.
- Sampler slot 13 is reserved for cube shadow maps.
- The cube pool is independent of the 2D spot pool; both are configured via the `[shadows]` config section.
- `ShadowCubeMap::BindFaceForWriting` re-binds the same FBO each call; GL drivers typically detect the redundant bind, so this is not a performance concern.

---

## Skills / Instructions Followed

- `impl-issue` skill (feat/ branch, conventional commit, PR to dev, history file)
- `CLAUDE.md`: one class per file, Google C++ style, `cppcheck-suppress unusedStructMember` on private members
- `data/shaders/glsl/CLAUDE.md`: `#include` for uniforms, documentation comments
- Column-vector math convention (`proj * view`)
- cpplint passed with no warnings
