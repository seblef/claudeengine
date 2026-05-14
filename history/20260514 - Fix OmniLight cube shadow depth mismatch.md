# Fix: OmniLight Cube Shadow Depth Mismatch

## Problem

Omni light (point light) shadows rendered with visible holes — areas that should have been in shadow were incorrectly lit.

## Root Cause

A depth representation mismatch between the shadow-map write pass and the shadow comparison in the lighting pass.

**What was stored** — The shared `shadow_depth` shader writes the hardware NDC depth, i.e. `gl_Position.z / gl_Position.w` after the perspective divide. For a perspective projection this is a non-linear function of the view-space z distance.

**What was compared** — `omni_light_ps.glsl` compared against:
```glsl
float compare = length(light_dir) / range;   // linear 3-D distance ∈ [0, 1]
```

These two representations are **fundamentally different**, so the `samplerCubeShadow` comparison almost always produced incorrect results, leaving random lit gaps where shadow should have been.

## Fix

Introduced a dedicated cube shadow depth shader pair (`shadow_depth_cube`) that writes linear 3-D distance to the light, normalised by range, as `gl_FragDepth`. This makes the stored depth directly comparable with the existing expression in `omni_light_ps.glsl` — no change to the lighting shader was needed.

### Changed files

| File | Change |
|------|--------|
| `data/shaders/glsl/uniforms/shadow_pass_infos.glsl` | Added `vec4 light_pos_range` (xyz = world pos, w = range) |
| `src/renderer/ShadowPassInfos.h` | Added `core::Vec4f light_pos_range`; updated static_assert 64 → 80 bytes |
| `src/renderer/ShadowRenderer.cpp` | Updated `kShadowPassInfosFloat4s` 4 → 5; fills `light_pos_range` per cube face; calls `RenderDepthCube()` |
| `src/renderer/MeshRenderer.h` | Declared `RenderDepthCube()` and `cube_depth_shader_` |
| `src/renderer/MeshRenderer.cpp` | Loads `shadow_depth_cube` shader; implements `RenderDepthCube()` |

### New files

| File | Purpose |
|------|---------|
| `data/shaders/glsl/shadow_depth_cube_vs.glsl` | Transforms vertex to clip space; outputs `v_world_pos` |
| `data/shaders/glsl/shadow_depth_cube_ps.glsl` | Writes `gl_FragDepth = length(v_world_pos - light_pos) / range` |

## Key Design Decisions

- **Separate shader, not a flag** — Adding a uniform flag to the existing `shadow_depth` shader to switch between NDC and linear depth would complicate the shared shader and break CSM/spot passes. A separate shader pair is cleaner and has zero overhead on non-cube passes.
- **`light_pos_range` as `vec4`** — Packing position (vec3) and range (float) into a single `vec4` avoids std140 padding issues. A bare `vec3` in std140 has 16-byte alignment, which would leave 4 bytes of waste between it and the following float; the `vec4` pack is tight and predictable on both C++ and GPU sides.
- **`ShadowPassInfos` extended, not forked** — The existing constant-buffer slot (6) is reused; non-cube passes simply leave `light_pos_range` uninitialised since their shaders never read it.

## Things to Keep in Mind

- Every omni light depth pass now runs through `RenderDepthCube()` and the dedicated shader — spot lights and CSM still use `RenderDepth()` with `shadow_depth`.
- The cube shadow map stores values in [0, 1] representing `dist / range`. Fragments beyond the light range therefore map to ≥ 1.0, which is always considered in shadow by the comparison — correct behaviour since they are outside the light volume anyway.
- The near plane for cube face projections is `range * 0.05f` (see `ShadowCubeMap::ComputeFaceMatrices`). Objects closer than 5% of the range to the light will be clipped; this is unchanged by this fix.
