# Shadow infrastructure — Depth32F render target format and SetViewport

**Issue**: #144  
**PR**: #153  
**Branch**: `feat/shadow-infrastructure`  
**Date**: 2026-05-13

## Changes

### `src/abstract/TextureFormat.h`
Added `kDepth32F` — 32-bit float depth-only texture format. Distinct from
`kDepth24Stencil8`: no stencil channel, intended exclusively for shadow maps
and `sampler2DShadow` hardware PCF.

### `src/abstract/VideoDevice.h`
Added `SetViewport(int x, int y, int width, int height)` pure virtual. The
shadow pass renders into shadow maps at their own resolution; `BeginFrame()`
continues to reset the viewport to full screen dimensions.

### `src/gldevices/GLRenderTarget.h/.cpp`
- `ToGLFormat`: added `kDepth32F` → `{GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT}`.
- Constructor: `kDepth32F` targets receive `GL_LINEAR` filter and
  `GL_COMPARE_REF_TO_TEXTURE` / `GL_LEQUAL` compare mode at creation time
  (not at bind time). This is the correct GL pattern — texture parameters
  are per-object state, not per-bind-point. Setting once avoids redundant
  state changes every frame.
- `BindAsOutput`: added `kDepth32F` branch attaching at `GL_DEPTH_ATTACHMENT`
  (not `GL_DEPTH_STENCIL_ATTACHMENT`).

### `src/gldevices/GLRenderTargetGroup.cpp`
- Constructor: when `color_targets` is empty, call `glDrawBuffer(GL_NONE)` /
  `glReadBuffer(GL_NONE)` after attaching the depth target. Required for
  depth-only FBO completeness per GL spec.
- `BindForWriting`: guard `glDrawBuffers` with `color_targets_.empty()` check;
  empty case calls `glDrawBuffer(GL_NONE)` to maintain correct state when
  the FBO is re-bound mid-frame.

### `src/gldevices/GLVideoDevice.h/.cpp`
Implemented `SetViewport` via `glViewport`.

## Decisions and rationale

**Why `kDepth32F` instead of reusing `kDepth24Stencil8`?**  
Shadow maps don't need a stencil channel. Using a pure depth format saves
4 bits/texel, and more importantly the `GL_DEPTH_COMPONENT` base format
is required for `sampler2DShadow` — the combined `GL_DEPTH_STENCIL` format
does not support shadow comparison sampling.

**Why configure compare mode in the constructor, not in `BindAsSampler`?**  
GL texture parameters are stored in the texture object, not the sampler binding
point. Setting them once at creation is correct and efficient. Setting them on
every `BindAsSampler` call would be wasteful redundant state changes.

**Depth-only FBO completeness**  
OpenGL requires `GL_DRAW_BUFFER = GL_NONE` when no color attachment is present,
otherwise `glCheckFramebufferStatus` returns `GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER`.
This must be set both at FBO construction (object state) and restored in
`BindForWriting` (to handle the case where the FBO is re-bound after another
FBO changed the draw buffer state — though GL FBO state is per-object, the
explicit call in `BindForWriting` makes the intent clear).

## Output to keep in mind

- `kDepth32F` render targets are ready for use as shadow map depth targets.
  Create via `video->CreateRenderTarget(size, size, TextureFormat::kDepth32F)`.
- Depth-only FBOs: pass an empty `std::span<RenderTarget*>` as `color_targets`
  to `CreateRenderTargetGroup`. The FBO will be complete.
- `SetViewport` must be called before each shadow map render pass, and the
  caller is responsible for restoring the screen viewport afterward (or rely
  on `BeginFrame()` at the start of the next frame).
- Sampler slot binding for `kDepth32F`: just call `BindAsSampler(slot)` as
  usual — compare mode is already configured. In GLSL, declare the sampler
  as `sampler2DShadow` and sample with `texture(u_shadow, vec3(uv, compare))`.

## Skills and instructions followed

- `src/CLAUDE.md`: one class per file, Google C++ style guide, include root is `src/`
- Root `CLAUDE.md`: conventional commits, cpplint before commit, PR to `dev`
