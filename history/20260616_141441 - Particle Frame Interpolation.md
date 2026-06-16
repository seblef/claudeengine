# Particle Frame Interpolation (Issue #578)

## Summary

Added smooth linear interpolation between consecutive sprite-sheet animation frames in particle emitters. When enabled, each particle renders a blended mix of the current and next frame's texture, eliminating the hard frame-cut that occurs at each animation tick.

## Changes

### `src/core/VertexParticle.h`
- Added two new per-vertex fields: `uv_offset2` (next-frame sprite UV) and `frame_blend` ([0,1] fractional sub-frame position).
- Updated the constructor to initialize both fields: `uv_offset2` defaults to `uv_offset` and `frame_blend` defaults to `0.f`, so existing code that doesn't set them yields the same result as before (no blending).
- Updated the layout comment to document the new offsets (total size grows from 48 to 64 bytes due to alignas(16)).

### `src/particles/ParticleSubSystemDesc.h`
- Added `bool smooth_transition = false;` after `animation_mode`.
- Documented that it is only effective in kSequential mode with `animation_fps > 0` and a multi-frame sprite sheet.

### `src/gldevices/GLVertexBuffer.cpp`
- Added GL vertex attribute configurations for location 5 (`uv_offset2`, vec2) and location 6 (`frame_blend`, float) in the `kParticle` branch of `ConfigureAttributes()`. Uses `offsetof()` so layout correctness is guaranteed regardless of struct padding.

### `src/particles/ParticleEmitter.cpp` — `UploadToGPU()`
- Refactored UV calculation to pre-compute `inv_cols` and `inv_rows` once per draw (minor efficiency gain).
- When `smooth_transition` is enabled (and conditions are met: sequential mode, fps > 0, frame_count > 1):
  - Computes the raw (floating-point) frame index as `p.age * animation_fps`.
  - Derives `frame_blend` as the fractional part via `frame_f - floor(frame_f)`.
  - Computes `uv_offset2` for `(p.frame + 1) % frame_count`.
- When disabled, `uv_offset2` and `frame_blend` keep their constructor defaults (`uv_offset2 = uv_offset`, `frame_blend = 0`), so the GPU performs `mix(sample0, sample0, 0) = sample0` with no visible change.

### `data/shaders/glsl/forward/particle_forward_vs.glsl`
- Added `in_uv_offset2` (location 5) and `in_frame_blend` (location 6) inputs.
- Computes `v_uv2` using the same corner UV mapping as `v_uv` but offset by `in_uv_offset2`.
- Passes `v_uv2` and `v_frame_blend` to the fragment shader.

### `data/shaders/glsl/forward/particle_forward_ps.glsl`
- Receives `v_uv2` and `v_frame_blend`.
- Samples the texture at both `v_uv` and `v_uv2`, blending with `mix()`:
  `tex_color = mix(texture(u_texture, v_uv), texture(u_texture, v_uv2), v_frame_blend)`
- When `v_frame_blend == 0`, identical to the previous single-sample behavior.

### `data/shaders/glsl/geometry/particle_gbuffer_vs.glsl` / `particle_gbuffer_ps.glsl`
- Same changes as the forward pass shaders.

### `src/particles/ParticleSystemTemplate.cpp`
- Added loading of `smooth_transition` in `ParseSubSystem()`.

### `src/editor/ParticleEditorWindow.cpp`
- Added serialization of `smooth_transition` in `SerializeToFile()`.
- Added a `Checkbox("Smooth transition")` control in the Sprite sheet section, disabled when conditions are not met (non-sequential mode, fps = 0, or single frame).

## Decisions

### Per-vertex vs. per-draw-call uniform approach
Embedding `uv_offset2` and `frame_blend` directly in the VBO (per vertex) rather than passing them as uniforms keeps the code symmetrical with the existing `uv_offset` and avoids adding uniform management to `ParticleRenderer`. The cost is 16 extra bytes per vertex (4 vertices × 4 bytes per particle), negligible relative to the particle counts typical in this engine.

### `frame_blend = 0` default = no cost
When `smooth_transition` is false, the GPU evaluates `mix(A, A, 0) = A`. The second texture fetch may be optimized away by the driver since `v_uv == v_uv2`. There is no behavioral change.

### Smooth transition disabled for random animation mode
Random-mode particles pick a fixed frame at birth and never advance through the animation. Frame blending has no meaning there. The editor disables the checkbox in this case; the CPU code guards with `animation_mode == kSequential`.

## What to keep in mind for next features

- `VertexParticle` is now 64 bytes (was 48). If future attributes are added, re-check alignment and update the layout comment.
- The two new GL attribute locations (5, 6) are occupied. Future vertex format extensions must start from location 7.
- `smooth_transition` only interpolates between adjacent frames. Non-linear or loop-wrap blending (e.g., frame N → frame 0 at end of animation) is handled naturally since `(frame + 1) % frame_count` wraps to 0.

## Skills and CLAUDE.md instructions used

- **`impl-issue` skill**: followed the workflow (dev checkout → branch → implement → cpplint → commit → PR).
- **`src/CLAUDE.md`**: one class/struct per file, Google C++ style, `#include` paths relative to `src/`.
- **`src/particles/CLAUDE.md`**: particles module must not be included by `abstract` or `core`; followed dependency graph.
- **`src/editor/CLAUDE.md`**: panel (`ParticleEditorWindow`) stays pure UI — no algorithm logic added, only checkbox and serialization.
- **`data/shaders/glsl/CLAUDE.md`**: vertex and pixel programs in separate files with matching names; `#include` for shared uniforms.
