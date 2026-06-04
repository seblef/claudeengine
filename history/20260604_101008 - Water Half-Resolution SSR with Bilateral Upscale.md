# Water: Half-Resolution SSR with Bilateral Depth-Aware Upscale (#386)

## Summary

Replaced the full-resolution screen-space reflection (SSR) ray march in the water
shader with a two-pass approach: the costly hierarchical ray march now runs at
half resolution, and the result is reconstructed at full resolution using a
cross-bilateral filter guided by the full-res depth buffer.

## Changes

### New files
- `data/shaders/glsl/water/water_ssr_vs.glsl` — half-res SSR vertex shader (Gerstner
  displacement + TBN, identical logic to `water_vs.glsl`).
- `data/shaders/glsl/water/water_ssr_ps.glsl` — half-res SSR fragment shader: runs
  the 8-step coarse + 4-step binary hierarchical ray march and outputs premultiplied
  alpha RGBA16F (`rgb = reflect_color * ssr_weight`, `a = ssr_weight`).

### Modified files
- `src/environment/WaterRenderer.h` — added `ssr_rt_`, `ssr_fbo_`, `ssr_shader_`,
  `screen_w_`/`screen_h_` members; `Resize(int w, int h)` method; `kSsrSlot = 4`
  constant; updated `Render()` signature to accept `output_fbo`.
- `src/environment/WaterRenderer.cpp` — two-pass render loop (`BuildSsrTarget()`,
  `Resize()`, updated `Render()`, `Reset()` extended).
- `src/renderer/EmissiveFBO.h` / `.cpp` — added `GetRenderTargetGroup()` accessor so
  WaterRenderer can rebind the emissive FBO after its SSR pre-pass.
- `src/renderer/Renderer.cpp` — updated `water_renderer_->Render()` call to pass
  `emissive_fbo_.GetRenderTargetGroup()`; added `water_renderer_->Resize(w, h)` to
  both `OnResize()` and `ResizeTargets()`.
- `data/shaders/glsl/water/water_ps.glsl` — replaced the 50+ line inline SSR loop
  with a 15-line cross-bilateral 2×2 upsample from `u_ssr_rt` (sampler slot 4).

## Architectural decisions

### Two-pass structure inside `WaterRenderer::Render()`
The entire two-pass sequence lives inside `Render()`. The method receives
`output_fbo` (the emissive FBO's underlying `RenderTargetGroup*`) so it can
rebind it for the main water pass after switching to the SSR FBO.

This avoids splitting the water logic across two call sites in `Renderer.cpp`,
and keeps the state machine (FBO binding, viewport changes, blend state) fully
encapsulated in `WaterRenderer`.

### No depth buffer for the SSR FBO
The half-res SSR FBO uses only a color attachment (no depth). The SSR pass
skips depth testing because:
1. Fragments occluded by other geometry still have zero-alpha SSR output, so
   they don't pollute the final image (the main pass rejects them via depth test).
2. Avoiding a half-res depth RT reduces memory allocation and FBO setup cost.

### Bilateral filter parameters
- **2×2 neighbourhood** (4 taps): minimal for half-to-full upsampling; enough
  to correct the dominant 2× misalignment.
- **`kDepthSigma = 200.0`**: with NDC depths in `[0,1]`, a depth difference of
  0.01 (≈ moderate geometry separation) produces `exp(-2) ≈ 0.14` weight, while
  0.005 produces `exp(-1) ≈ 0.37`. This gives sharp edge rejection without
  requiring per-scene tuning.

### Normal map omitted in SSR pass
`water_ssr_ps.glsl` uses only the Gerstner macro normal (from the vertex shader)
for the reflection direction, without sampling the normal map. The micro-ripple
perturbation from the normal map is a secondary effect that adds noise to
half-res SSR; the macro Gerstner normal dominates the reflection direction.

### `Resize()` lifecycle
`Build()` reads `video_->GetWidth()/GetHeight()` and calls `BuildSsrTarget()`.
`Resize()` recreates `ssr_fbo_` and `ssr_rt_` in place without GPU leaks (the
old unique_ptrs are reset before allocation).

## Output / notes for future features

- `u_ssr_rt` is at sampler slot 4 in `water_ps.glsl`; the next available slot is 5.
- `ssr_fbo_` has no depth attachment; if a future pass needs depth testing in the
  SSR pre-pass, a half-res depth RT must be added to `BuildSsrTarget()`.
- The SSR ray march parameters (8 coarse steps × 16 m, 4 binary refinement steps)
  are duplicated between `water_ssr_ps.glsl` and the former `water_ps.glsl`. They
  should remain consistent.

## Skills / CLAUDE.md instructions followed

- `src/CLAUDE.md`: one class per `.h`/`.cpp`, include paths from `src/`.
- `src/environment/CLAUDE.md`: `environment` must not depend on `renderer`; confirmed
  — `WaterRenderer` depends only on `abstract` (via `RenderTargetGroup`).
- `data/shaders/glsl/CLAUDE.md`: each shader has matching `_vs.glsl` / `_ps.glsl`
  files; custom `#include` used for uniform block declarations.
- Git workflow: branched from `dev`, `cpplint` clean, PR to `dev`.
