# EditorViewport: Scene Rendering to ImGui Panel

**Date:** 2026-05-15
**Issue:** #169
**Branch:** feat/editor-viewport-scene-texture
**Skills used:** impl-issue

---

## Summary

`EditorViewport` now renders the scene each frame into an offscreen texture and
displays it inside the ImGui "Viewport" panel via `ImGui::Image()`.

---

## Changes

### `editor/EditorViewport.h` / `.cpp` (rewritten)

- Owns a `game::GameCamera` (perspective, right-handed, 45° FOV, 0.1–1000 range)
  and an `EditorCameraController`, both constructed at startup.
- Owns `render_target_` (RGBA8) and `render_fbo_` (single-color RenderTargetGroup
  wrapping it). Both are null until the first `Render()` call sets a panel size.
- `ResizeIfNeeded(w, h)`: detects panel-size changes, calls
  `Renderer::ResizeTargets()`, recreates the RT and FBO.
- `Render()` (called inside ImGui::Begin("Viewport")):
  1. `GetContentRegionAvail()` → w/h, clamped to ≥ 1.
  2. `SetViewportHovered` + `Update` on camera controller.
  3. `ResizeIfNeeded` to rebuild RT/FBO if the panel resized.
  4. `Renderer::Update(time, camera, fbo)` renders into the texture.
  5. `ImGui::Image(handle, avail, {0,1}, {1,0})` blits with Y-flip.
- `OnEvent(e)`: forwards to the camera controller (called by EditorWindow).

### `editor/EditorWindow.h` / `.cpp`

- Constructor now takes `abstract::VideoDevice*` and passes it to `EditorViewport`.
- Added `OnEvent(const core::Event&)` forwarding to `viewport_->OnEvent()`.
- Viewport `ImGui::Begin` now uses
  `ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse`.

### `editor/EditorSystem.cpp`

- Passes `video_` to `EditorWindow`.
- Dispatches every consumed event to `editor_window_->OnEvent(e)` so the camera
  controller sees mouse and keyboard events.

---

## Design decisions

**RGBA8 for the offscreen RT**

The composite pass already gamma-corrects from HDR, so the final output is LDR
8-bit. RGBA16F would waste bandwidth with no quality benefit here.

**No depth attachment on render_fbo_**

The FBO passed to `Renderer::Update()` is for the composite pass only (step 4 of
the pipeline). The GBuffer and emissive FBO have their own depth attachments.
Adding a depth attachment here would be wasteful and incorrect.

**Y-flip via ImGui UV**

OpenGL FBOs have origin at bottom-left; ImGui/screen origin is top-left. Passing
`uv0={0,1}`, `uv1={1,0}` is the zero-cost fix: no texture copy, no shader change.

**Event dispatch through the call chain**

`EventManager` is a pure queue with no listener registry. Events are dispatched
manually: `EditorSystem` → `EditorWindow::OnEvent` → `EditorViewport::OnEvent` →
`EditorCameraController::OnEvent`. This keeps the event path explicit and avoids
adding listener infrastructure to `core`.

---

## Notes for future work

- Issue #170 (EditorScene) will set `scene_` on `EditorViewport` once a scene
  object exists; scene renderables are added/removed through it.
- `ImTextureRef(void*)` constructor (ImGui 1.92+) handles the `GetNativeHandle()`
  → `ImTextureID` conversion automatically — no explicit cast needed at call site.
- The shadow debug overlay (`ShadowDebugRenderer`) currently always draws to the
  default framebuffer, not into the render texture. This is intentional: it's a
  developer tool and is not visible in the editor viewport panel.
