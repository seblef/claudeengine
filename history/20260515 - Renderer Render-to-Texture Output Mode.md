# Renderer: Render-to-Texture Output Mode

**Date:** 2026-05-15
**Issue:** #168
**Branch:** feat/renderer-render-to-texture
**Skills used:** impl-issue

---

## Summary

Added render-to-texture support to the renderer so the editor viewport can display the scene inside an ImGui panel via `ImGui::Image()`.

---

## Changes

### `abstract/RenderTarget.h`
- Added pure virtual `GetNativeHandle() const → void*` to expose the backend texture handle. On OpenGL this returns `(void*)(intptr_t)tex_id_`, directly usable as the `ImTextureID` argument to `ImGui::Image()`.

### `gldevices/GLRenderTarget.h` / `.cpp`
- Declared and implemented `GetNativeHandle()` returning `reinterpret_cast<void*>(static_cast<intptr_t>(tex_id_))`.

### `renderer/Renderer.h`
- Changed `Update()` signature: `void Update(float time, const core::Camera* camera, abstract::RenderTarget* output = nullptr)`.
  - `output = nullptr` preserves the existing game-path behaviour unchanged.
- Added `void ResizeTargets(int w, int h)` for viewport-panel-driven resize.
- Added private `last_output_rt_` / `output_fbo_` for FBO caching.

### `renderer/Renderer.cpp`
- Output FBO is cached: recreated only when the `output` pointer changes frame-to-frame (avoids a new GL FBO allocation every frame).
- Both draw paths (normal composite pass and G-buffer debug bypass) wrap their `RenderIndexed()` call with `output_fbo_->BindForWriting()` / `UnbindForWriting()` when an output is set.
- `ResizeTargets()` resizes GBuffer and emissive FBO, then resets the output FBO cache so the next frame gets a fresh FBO matching the new resolution.

---

## Design decisions

**`GetNativeHandle()` instead of `GetTexture()`**

The issue asked for `abstract::Texture*` but `abstract::Texture` has a resource-registry lifecycle (loaded from disk, ref-counted) incompatible with render targets. `ImGui::Image()` only needs a `void*` texture ID, which `GetNativeHandle()` provides directly without introducing a fake Texture wrapper.

**FBO caching**

`RenderTargetGroup` (a GL FBO) is cheap to create but not free. Caching by pointer identity avoids one allocation per frame in the editor while keeping the API simple (caller just passes the RT every frame).

**`output = nullptr` default**

The game path calls `Update(time, camera)` with no third argument — adding a defaulted parameter is fully backward-compatible and requires zero changes to game-side callers.

---

## Notes for future work

- `EditorViewport` (issue #169) consumes this: create a `RenderTarget` at viewport panel size, pass to `Renderer::Update()`, then `ImGui::Image(rt->GetNativeHandle(), ...)`.
- Call `Renderer::ResizeTargets(w, h)` from the viewport when `ImGui::GetContentRegionAvail()` changes.
- The shadow debug overlay (`shadow_debug_renderer_->Render(...)`) currently always draws to the default framebuffer even when `output_fbo_` is set. For a pure editor viewport this is fine (the overlay is a developer tool), but if it needs to render into the texture too, wrap it the same way as the composite pass.
