# Renderable Infos Constant Buffer

**Date:** 2026-05-07
**Branch:** feat/renderable-infos-cb
**Issue:** #74

## Summary

Standardised per-object GPU data binding by introducing a `RenderableInfos` constant buffer managed by the `Renderer` class. The world matrix (previously a raw CB in `main.cpp`) is now uploaded through `Renderer::SetRenderableInfos()`. Both per-object and per-frame CBs are bound once per frame in `Update()`. A new GLSL uniform file standardises the shader-side binding.

## Slot Layout (Before → After)

| Slot | Before | After |
|------|--------|-------|
| 0 | world matrix (raw CB in `main.cpp`) | *(free)* |
| 1 | SceneInfosBlock | **RenderableInfosBlock** (world matrix) |
| 2 | *(free)* | SceneInfosBlock |

## Changes

### New: `src/renderer/RenderableInfos.h`

GPU struct matching `layout(std140, row_major)` with a single `core::Mat4f world` field (64 B = 4 float4s). Follows the same pattern as `SceneInfos.h`. Has a `static_assert(sizeof == 64)`.

### New: `data/shaders/glsl/uniforms/renderable_infos.glsl`

GLSL uniform block at `binding = 1`:
```glsl
layout(std140, row_major, binding = 1) uniform RenderableInfosBlock {
    mat4 world;
};
```

### Modified: `src/renderer/Renderer.h`

- Added `SetRenderableInfos(const core::Mat4f&)` — public method
- Added `renderable_infos_cb_` (`unique_ptr<ConstantBuffer>`) — private member
- Updated class-level doc comment to document both slots (1 = renderable infos, 2 = scene infos)
- Added `#include "core/Mat4f.h"`

### Modified: `src/renderer/Renderer.cpp`

- `kSceneInfosSlot` changed from 1 → 2
- Added `kRenderableInfosSlot = 1`, `kRenderableInfosFloat4s = 4`
- Constructor creates both CBs; **no `Bind()` calls** in constructor
- `Update()` now calls `renderable_infos_cb_->Bind()` and `scene_infos_cb_->Bind()` before filling scene infos
- New `SetRenderableInfos()` fills `RenderableInfos{world_matrix}` into the CB

### Modified: `data/shaders/glsl/uniforms/scene_infos.glsl`

`binding = 1` → `binding = 2`

### Modified: `data/shaders/glsl/textured_mesh_vs.glsl`

- Removed inline `WorldBlock` (binding 0)
- Added `#include <uniforms/renderable_infos.glsl>` (binding 1)
- The `world` identifier is preserved (from `RenderableInfosBlock`), so `view_proj * world * vec4(in_position, 1.0)` compiles unchanged

### Modified: `src/app/main.cpp`

- Removed manual `world_cb = CreateConstantBuffer(4, 0)` and its `Bind()` call
- Removed `world_cb->Fill(world.Data())`
- Added `renderer.SetRenderableInfos(world)` in the render loop
- Ordering: `Update()` first (binds both CBs + fills scene infos), then `SetRenderableInfos()` (fills per-object CB)

## Decisions & Rationale

### Scene infos moves to slot 2
The issue explicitly assigns renderable infos to slot 1. Slot 1 was previously scene infos — moving it to slot 2 is the only conflict-free choice.

### `SetRenderableInfos()` does not call `Bind()`
Binding is a one-time setup for the frame (done in `Update()`). Calling `Bind()` every draw would be wasteful. The fill-per-draw pattern mirrors how `FillSceneInfos()` works for the scene CB.

### `RenderableInfos` is a dedicated struct
Consistent with `SceneInfos`. Future extensions (e.g. a pre-computed normal matrix for non-uniform scaling) add fields to the struct without changing call sites.

### `SetCamera()` left unchanged
`SetCamera()` still calls `FillSceneInfos()` as a convenience (it doesn't bind). The issue only asked to move the `Bind()` call, not restructure the entire update path.

## Skills & Instructions Used

- `impl-issue` skill (CLAUDE.md git workflow)
- Root `CLAUDE.md`: conventional commits, history file, cpplint, PR to dev
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`
- `data/shaders/glsl/CLAUDE.md`: uniforms in `uniforms/` subfolder, use `#include`
