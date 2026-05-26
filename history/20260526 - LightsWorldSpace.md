# Lights in World Space (Issue #281)

## Summary

Moved editor light wireframe rendering and selection picking from screen space to world space, enabling correct depth testing (z-buffering) so lights behind opaque geometry are properly occluded.

## Changes

### New: `LightWireframeRenderer` (`src/editor/LightWireframeRenderer.h/.cpp`)

New editor-module class that renders all non-global light wireframes as actual 3D GPU geometry with depth testing. Key design decisions:

- **Dynamic vertex buffer** (`VertexBase`, `kDynamic`): line list built per-frame from scene lights on the CPU, uploaded to GPU; capacity grows by power-of-two as needed.
- **Depth-aware FBO**: the renderer is called with a `RenderTargetGroup` that combines the scene colour RT with the GBuffer depth attachment (same pattern as `EmissiveFBO`). Depth write is disabled so the light geometry does not corrupt the scene depth.
- **Shader `light_wireframe`**: vertex shader transforms world-space positions by `view_proj` from `SceneInfosBlock` (UBO slot 2, already bound by `Renderer::Update`); fragment shader outputs vertex colour verbatim.
- **Colour convention**: unselected lights are yellow `(1.0, 0.8, 0.0)`; selected light is blue `(0.0, 0.53, 1.0)`.

### New shaders: `light_wireframe_vs.glsl` / `light_wireframe_ps.glsl`

Minimal world-space line shader. Vertices are supplied in world space so no per-object world matrix uniform is needed.

### Modified: `EditorViewport`

- **`wireframe_fbo_`** added: a `RenderTargetGroup` combining `render_target_` colour with GBuffer depth. Created/recreated in `ResizeIfNeeded` right after `ResizeTargets` (which recreates the GBuffer, invalidating the old depth pointer).
- **`light_wireframe_`** member: `LightWireframeRenderer`, initialised with the video device.
- **Render loop**: replaced the ImGui draw-list `DrawLightsOverlay` call with `light_wireframe_.Render(…, wireframe_fbo_.get())` executed after `Renderer::Update`. The light geometry is now rendered into the scene FBO before it is shown as an ImGui image.
- **`PickObjectAt`**: replaced screen-space proximity picking (pixel-distance to projected wireframe) with world-space ray-bbox intersection (`GetWorldBBox().IntersectsRay`), identical to the mesh picking path. The hit distance `t` is now directly comparable to mesh triangle hits, giving correct depth ordering (the closest hit along the ray wins).
- Removed unused anonymous-namespace helpers: `DrawLineSegment3D`, `DrawCircle3D`, `DrawLightWireframe`, `ScreenDistToSegment`, `ScreenPt`, `ProjectToScreen`, `LightPickDistPx`.

## Rationale

The previous approach used ImGui's draw list to overlay light wireframes on top of the rendered scene. ImGui draws happen after the render pass and have no access to the depth buffer, so lights always appeared on top of geometry regardless of their actual world position, making the editor misleading.

By rendering light wireframes as first-class GPU geometry into the scene FBO (with depth test enabled, depth write disabled), lights behind walls or floors are correctly hidden. The wireframe FBO re-uses the existing GBuffer depth RT (no extra allocation) following the same pattern already used by `EmissiveFBO`.

For picking, screen-space distance was fragile (ambiguous when two lights project close together, and inconsistent with mesh picking depth ordering). World-space bbox intersection gives deterministic closest-hit semantics consistent with the mesh pipeline and correctly handles the case where a mesh is in front of a light.

## Outputs to keep in mind

- The `wireframe_fbo_` borrows the GBuffer depth RT pointer; it **must** be recreated in `ResizeIfNeeded` after `ResizeTargets` to track the new pointer.
- The `SceneInfosBlock` UBO (slot 2) remains bound after `Renderer::Update` returns, so the wireframe shader can consume it without re-binding.
- The `VertexBase` layout (position + color + uv) is used even though UV is unused — this matches the existing GPU layout `base_vertex.glsl` and avoids a custom vertex type.
- Light picking now selects anything whose world-space bbox the ray intersects; very large lights (wide-angle spotlights with long range) have large bboxes. If this causes false positives the picking could be refined to ray-sphere / ray-cone tests, but the current approach is sufficient for normal use.

## Skills and CLAUDE.md instructions used

- `src/CLAUDE.md` — one class per .h/.cpp pair, include root is `src/`, Google C++ style
- `src/editor/CLAUDE.md` — editor is leaf of dependency graph (never imported by game/renderer)
- `data/shaders/glsl/CLAUDE.md` — separate `_vs` / `_ps` files, custom `#include` for layouts/uniforms
