# Particles — ParticleRenderer forward pass (kAdditive & kAlphaBlend)

## Summary

Added `RenderForwardPass()` to `ParticleRenderer` so that transparent particle
emitters (fire sparks, smoke, etc.) are rendered correctly into the HDR emissive
render target using forward blending. Closes issue #451.

## Changes

### `data/shaders/glsl/forward/particle_forward_vs.glsl` (new)

Billboard vertex shader for the forward pass. Identical expansion logic to
`particle_gbuffer_vs.glsl`: extracts camera right/up from the view matrix rows
and expands each particle center into a world-space quad. Outputs `v_uv` and
`v_color` to the fragment shader.

### `data/shaders/glsl/forward/particle_forward_ps.glsl` (new)

Fragment shader for the forward pass. Single HDR render target (no MRT).

- **Unlit** (`u_lit == 0`): outputs `tex_color * v_color` directly.
- **Lit** (`u_lit == 1`, `kAlphaBlend + desc.lit`): computes a world-space
  camera-facing billboard normal (`-view[2].xyz`, consistent with
  `particle_gbuffer_ps.glsl`), dots it with the to-light vector from
  `LightInfosBlock` (slot 4), and applies
  `ambient + color * intensity * diff`.

`kAdditive` particles never apply lighting — they are emissive by nature.
Includes `scene_infos.glsl` (view matrix) and `light_infos.glsl`.

### `src/particles/ParticleEmitter.h`

Added `GetWorldPosition()` accessor (inline, returns `origin_`) so
`ParticleRenderer` can sort `kAlphaBlend` emitters back-to-front without
accessing the private `origin_` field.

### `src/particles/ParticleRenderer.h / .cpp`

- Renamed `shader_` → `gbuffer_shader_`, added `forward_shader_` (loads
  `"forward/particle_forward"`).
- Added `RenderForwardPass(const core::Camera&, abstract::ConstantBuffer*)`.
- Updated class-level doc comment.

`RenderForwardPass`:
1. Separates registered emitters into `additive_emitters` and `alpha_emitters`.
2. Sorts `alpha_emitters` back-to-front by squared distance from `camera.GetPosition()`.
3. Draws additive emitters with `GL_ONE / GL_ONE` blend, `u_lit = 0`.
4. Draws alpha-blend emitters with `GL_SRC_ALPHA / GL_ONE_MINUS_SRC_ALPHA` blend,
   `u_lit = desc.lit ? 1 : 0`.
5. Restores blend/depth state on exit.

### `src/renderer/Renderer.cpp / .h`

- Added step **4b** in `Update()`: calls `particle_renderer_->RenderForwardPass()`
  inside the emissive FBO binding, after foliage/tree billboard draws and before
  `emissive_fbo_.UnbindForWriting()`.
- Updated pipeline comment in `Renderer.h` to document step 4b.
- Former step 4b (water pass) renamed to **4c** in comments.

## Decisions

- **Shader uniform `u_lit` as `int`** — `abstract::Shader` only exposes
  `SetUniformInt / SetUniformFloat / SetUniform2f`; using `int` avoids adding
  a new virtual method.
- **`bool u_lit` → `int u_lit` in GLSL** — matches the CPU-side `SetUniformInt`
  call; GLSL comparison `u_lit != 0` is equivalent to a bool test.
- **Lighting uses `LightInfosBlock` (slot 4), not SkyInfosBlock (slot 8)** —
  the issue's pseudo-code refers to `sky_ambient_color` / `sun_color` but
  `SkyInfosBlock` does not carry those values. `LightInfosBlock` is the correct
  source (`ambient`, `color * intensity`), and `LightRenderer::Instance().BindGlobalLight()`
  is already called before the billboard passes in the emissive pass.
- **Camera-facing normal derived from view matrix** — same approach as
  `particle_gbuffer_ps.glsl` (`-view[2].xyz`). Physically correct and consistent
  with the G-buffer particle normal.
- **Particle forward pass placed before water pass** — the issue places it after
  the emissive pass and before the composite pass. Water already comes after
  emissive, so particle forward is inserted between emissive and water.

## Output to keep in mind

- A `forward/` subfolder was created under `data/shaders/glsl/` for forward-pass
  shaders. Future forward-pass shaders (water, foliage if ever moved) can live
  here.
- `GetWorldPosition()` on `ParticleEmitter` is intentionally inline and trivial.
  It is only used for sorting, not per-particle per-vertex work.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h`, Google C++ style, no platform/GL headers.
- `src/particles/CLAUDE.md`: no platform or OpenGL headers, go through `abstract/`.
- `data/shaders/glsl/CLAUDE.md`: paired `_vs.glsl` / `_ps.glsl` naming, `#include`
  for uniforms, documentation in comments.
