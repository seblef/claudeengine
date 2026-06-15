# Integrate WireframeRenderer into Renderer lifecycle — Issue #525

## Overview

Wires the `WireframeRenderer` singleton (introduced in issue #524) into `Renderer`
so that its frame lifecycle is driven automatically and its configuration is
reachable via the `Renderer` public API. Removes the now-redundant
`RenderTerrainWireframe()` method.

## Changes

### `renderer/Renderer.h`
- Added `#include "renderer/WireframeRenderer.h"`.
- Added three `static` forwarding methods:
  - `SetGizmosEnabled(bool)`
  - `SetTerrainWireframeEnabled(bool)`
  - `SetHighlightedObject(const void*)`
  They are `static` because they forward to a singleton and never access `this`;
  cppcheck flagged the non-static form as a style violation.
- Removed `RenderTerrainWireframe()` declaration and its doc comment.

### `renderer/Renderer.cpp`
- **Constructor**: `new WireframeRenderer(video_)` added after the existing
  `MeshRenderer` / `LightRenderer` / `ShadowRenderer` allocations.
- **Destructor**: `WireframeRenderer::Shutdown()` called before the other
  `Shutdown()` calls (LIFO destruction order).
- **`Update()`**: `WireframeRenderer::Instance().BeginFrame()` added at the top
  alongside `LightRenderer::Instance().EndRender()` and
  `MeshRenderer::Instance().EndRender()` so both vertex lists are cleared each
  frame before the visibility pass re-enqueues geometry.
- **`SetTerrainRenderer()`**: additionally calls
  `WireframeRenderer::Instance().SetTerrainRenderer(terrain)` so the terrain
  pointer stays in sync whenever a terrain is registered or detached.
- Removed `RenderTerrainWireframe()` implementation.

### `editor/EditorViewport.cpp`
- Added `#include "renderer/WireframeRenderer.h"`.
- Calls `Renderer::SetTerrainWireframeEnabled(terrain_wireframe_debug_)` before
  `Renderer::Update()` to keep the flag in sync each frame.
- Replaced the old `if (terrain_wireframe_debug_) RenderTerrainWireframe(...)` block
  with an unconditional `WireframeRenderer::Instance().Render(camera, wireframe_fbo_,
  render_fbo_)` call (terrain wireframe is now gated inside WireframeRenderer via its
  `terrain_wireframe_enabled_` flag).

## Decisions

- **Static forwarding methods**: The three new `Renderer` methods forward to
  `WireframeRenderer::Instance()` without touching any `Renderer` member. Making
  them `static` is semantically correct and avoids cppcheck's `functionStatic`
  diagnostic.
- **`WireframeRenderer::Render()` call site stays in the editor**: The editor owns
  the wireframe FBO and decides when to draw the overlay; calling `Render()` inside
  `Renderer::Update()` would require the FBO to be known to the renderer, which is
  an unnecessary coupling. The BeginFrame / Render split is the same pattern as
  LightRenderer / MeshRenderer.

## Skills and CLAUDE.md rules applied

- `impl-issue` skill for branch/PR workflow.
- Conventional commits format for the commit message.
- Google C++ style guide (static methods, include order).
- History file in `history/` as required by project CLAUDE.md.
