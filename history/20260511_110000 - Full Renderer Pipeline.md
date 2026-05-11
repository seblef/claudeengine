# Full 4-Phase Renderer Pipeline

**Issue**: #85
**Branch**: feat/full-renderer-pipeline

## Overview

Wires the geometry pass, lighting pass, emissive pass, and HDR composite pass into a single `Renderer::Update()` call. Also introduces `LightRenderer` lifecycle management, the composite shaders, and two new `VideoDevice` render-state methods.

## Files Changed

### New files
- `src/abstract/BlendFactor.h` — `kZero`, `kOne`, `kSrcAlpha`, `kOneMinusSrcAlpha`
- `data/shaders/glsl/composite_vs.glsl` — NDC fullscreen quad passthrough
- `data/shaders/glsl/composite_ps.glsl` — linear-to-sRGB gamma correction

### Modified files
- `src/abstract/VideoDevice.h` — two new pure virtuals: `SetBlendEnabled()`, `SetDepthWriteEnabled()`
- `src/gldevices/GLVideoDevice.h/.cpp` — implementations of both
- `src/renderer/Renderer.h` — composite shader + quad members; updated doc comment
- `src/renderer/Renderer.cpp` — full 4-phase `Update()`; `LightRenderer` lifecycle

## Key Decisions

### HDR RT clear without clobbering G-buffer depth

Both the HDR RT and the G-buffer depth are attached to `emissive_fbo_` simultaneously. At the start of the lighting pass we need to zero-out the HDR RT but leave G-buffer depth intact (the stencil sub-passes depend on it).

`glDepthMask(GL_FALSE)` causes `glClear(GL_DEPTH_BUFFER_BIT)` to be a no-op even when the bit is included in the clear mask (this is explicitly allowed by the OpenGL spec, §4.2.3). So calling `SetDepthWriteEnabled(false)` before `ClearRenderTargets` clears only the HDR color attachment. No separate `ClearColorOnly` API was needed.

### Depth write disabled across lighting + emissive passes

`SetDepthWriteEnabled(false)` is set before the lighting pass and stays disabled through the emissive pass. This serves two purposes:
1. Prevents stencil sub-pass A from corrupting G-buffer depth during light volume drawing.
2. Makes emissive surfaces depth-test-correct (they appear behind opaque geometry) without overwriting depth with their fragment depths.

Depth write is restored to `true` at the end of `Update()`.

### Depth texture as both FBO attachment and sampler

The G-buffer depth RT is simultaneously attached to `emissive_fbo_` as a depth+stencil attachment and bound as sampler slot 8 for position reconstruction in the lighting shaders. This is a common deferred shading pattern; with depth writes disabled the driver treats the attachment as "read-only" and the sampler read is well-defined on all GL 4.x implementations.

### LightRenderer lifecycle

Created in `Renderer` constructor after `MeshRenderer`; destroyed (via `Shutdown()`) in the destructor before `MeshRenderer`. This mirrors the existing pattern for `MeshRenderer`.

### MeshRenderer::EndRender() call timing

`PrepareRender()` and `Render()` happen in the geometry pass. `RenderEmissive()` needs the same sorted instance list built by `PrepareRender()`, so `EndRender()` (which clears `instances_`) is deferred until after the emissive pass.

## What's Next

- Issue #86: Debug G-buffer view modes (CPU-side bypass, `debug_blit_ps.glsl`).
- Issue #87: Demo scene — cube + sphere with all four light types + debug key.
