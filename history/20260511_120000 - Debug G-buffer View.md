# Debug G-buffer View (Issue #86)

## Summary

Added a CPU-side debug mode to `Renderer` that lets developers inspect each
G-buffer attachment during development of the deferred shading pipeline. All
debug logic lives on the CPU; no branches were introduced into any production
shader.

## Changes

### `src/renderer/SceneInfos.h` + `data/shaders/glsl/uniforms/scene_infos.glsl`

Extended `SceneInfos` from 352 to 368 bytes (22 → 23 float4s) by appending
`z_near_`, `z_far_`, and two padding floats at offset [352]. These are filled
by `FillSceneInfos()` from `camera_->GetMinDepth()` / `GetMaxDepth()` and
consumed by `debug_blit_ps.glsl` to linearize the raw depth buffer for display.
The GLSL UBO block was updated to match exactly.

### `src/abstract/Shader.h` + `src/gldevices/GLShader.h/.cpp`

Added `SetUniformInt(const std::string& name, int value)` as a pure virtual on
`abstract::Shader`. `GLShader` implements it via `glGetUniformLocation` +
`glUniform1i`. `glGetUniformLocation` is called each invocation, which is
acceptable for the debug-only path where performance is not critical.

### `src/renderer/Renderer.h/.cpp`

- `DebugMode` enum (`kNone`, `kAlbedo`, `kNormal`, `kSpecular`, `kDepth`)
  declared at namespace scope so callers don't need to qualify with a class name.
- `SetDebugMode(DebugMode)` inline setter.
- `debug_shader_` member acquires the `"debug_blit"` shader in the constructor
  and releases it in the destructor (same pattern as `composite_shader_`).
- In `Update()`, after `gbuffer_.UnbindForWriting()`, if `debug_mode_ != kNone`:
  1. Select the appropriate RT (`GetAlbedoRT()`, `GetNormalRT()`, etc.)
  2. Bind to sampler slot 0
  3. Activate `debug_shader_`, pass `u_debug_mode` via `SetUniformInt`
  4. Draw `composite_quad_` (reused — no new geometry needed)
  5. Call `MeshRenderer::Instance().EndRender()` and
     `LightRenderer::Instance().EndRender()` to flush accumulated instance lists
  6. Return early — lighting, emissive, and composite passes are skipped
- `FillSceneInfos()` now also fills `si.z_near_` and `si.z_far_`.

### `data/shaders/glsl/debug_blit_vs.glsl` + `debug_blit_ps.glsl`

- VS is identical to `composite_vs.glsl` — NDC fullscreen quad passthrough
  outputting `v_uv`.
- PS includes `uniforms/scene_infos.glsl` for `z_near`/`z_far` and branches on
  `u_debug_mode`:
  - 1 (albedo): pass-through RGB
  - 2 (normal): decode from [0,1] → [-1,1] for display as colour
  - 3 (specular): R = specular intensity, G = shininess × 4
  - 4 (depth): NDC depth → linear view-space depth → normalized [0,1]

## Decisions and Rationale

**`DebugMode` as namespace-scope enum rather than nested class enum**: callers
(`main.cpp`, editor code) can write `renderer::DebugMode::kAlbedo` without
needing `renderer::Renderer::DebugMode::kAlbedo`. Cleaner ergonomics.

**`u_debug_mode` via `glUniform1i` rather than a tiny UBO**: avoids allocating
a constant buffer for a single int used only on the debug path. The
`glGetUniformLocation` call per frame is negligible; this path is never hot.

**Reuse `composite_quad_`**: the debug blit uses the same NDC fullscreen quad.
No additional `GeometryData` member needed.

**Both `EndRender()` calls in the debug early-return**: `PrepareRender()` builds
the sorted instance list that both `Render()` and `RenderEmissive()` consume.
If `Update()` returns early after the geometry pass, both `MeshRenderer` and
`LightRenderer` still hold accumulated lists from `Enqueue()` calls. Calling
`EndRender()` clears them so the next frame starts clean.

## Output to Keep in Mind

- `SceneInfos` is now 368 bytes (23 float4s). Any future field additions must
  respect 16-byte alignment at the next slot boundary (offset 368).
- `Shader::SetUniformInt()` is now part of the abstract interface — future
  backends (DirectX) must implement it.
- The depth linearization formula in `debug_blit_ps.glsl` assumes standard
  OpenGL NDC range [-1, 1] for z. Verify if ever porting to Vulkan (NDC [0, 1]).

## PR

https://github.com/seblef/claudeengine/pull/95

## Skills Used

- `impl-issue`
- `plan-milestone`
