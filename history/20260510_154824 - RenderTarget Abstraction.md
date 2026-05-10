# RenderTarget Abstraction and OpenGL FBO Implementation

**Issue:** #79  
**Branch:** feat/render-target-abstraction  
**Date:** 2026-05-10

## Summary

Added the foundational off-screen rendering infrastructure required by the deferred shading pipeline. The engine now supports:
- Off-screen render targets (color, HDR, depth+stencil formats)
- FBO bundles grouping N color targets + 1 depth+stencil target
- New render-state controls: stencil test, face culling, color write mask

## Changes

### New abstract headers (`src/abstract/`)
| File | Purpose |
|---|---|
| `RenderTarget.h` | Abstract off-screen GPU texture (color or depth+stencil) |
| `RenderTargetGroup.h` | Abstract FBO bundle |
| `CullFace.h` | Enum: `kNone`, `kFront`, `kBack` |
| `StencilOp.h` | Enum: `kKeep`, `kZero`, `kIncrWrap`, `kDecrWrap` |
| `CompareFunc.h` | Enum: `kAlways`…`kGreaterEqual` (8 values) |
| `Face.h` | Enum: `kFront`, `kBack` (per-face stencil ops) |

### Modified `src/abstract/TextureFormat.h`
Added three render-target-specific formats:
- `kRGBA8` — albedo and specular G-buffer (GL_RGBA8)
- `kRGBA16F` — normal G-buffer and HDR accumulation (GL_RGBA16F)
- `kDepth24Stencil8` — combined depth+stencil for G-buffer (GL_DEPTH24_STENCIL8)

### Modified `src/abstract/VideoDevice.h`
Added pure virtual methods:
- `SetColorWriteEnabled(bool)` — mask all color channels (for stencil-fill sub-pass)
- `SetFaceCulling(CullFace)` — configure GL_CULL_FACE
- `SetStencilTestEnabled(bool)` — toggle GL_STENCIL_TEST
- `SetStencilFunc(CompareFunc, int ref, unsigned mask)` — stencil comparison
- `SetStencilOp(Face, StencilOp sfail, StencilOp dpfail, StencilOp dppass)` — per-face operations
- `ClearStencil(int val)` — clear stencil buffer
- `CreateRenderTarget(w, h, format)` — factory returning unique_ptr
- `CreateRenderTargetGroup(span<RenderTarget*>, RenderTarget*)` — FBO factory

### New GL implementation (`src/gldevices/`)
| File | Purpose |
|---|---|
| `GLRenderTarget.h/.cpp` | Wraps `GL_TEXTURE_2D`; `BindAsSampler` → `glActiveTexture`; `BindAsOutput` → `glFramebufferTexture2D` |
| `GLRenderTargetGroup.h/.cpp` | Wraps `GLuint fbo_`; attaches all targets at construction; verifies FBO completeness |

`GLVideoDevice` now implements all new factory and render-state methods.

### Format mapping (GLRenderTarget)
| TextureFormat | GL internal | GL base | GL type |
|---|---|---|---|
| `kRGBA8` | GL_RGBA8 | GL_RGBA | GL_UNSIGNED_BYTE |
| `kRGBA16F` | GL_RGBA16F | GL_RGBA | GL_HALF_FLOAT |
| `kG11R11B10F` | GL_R11F_G11F_B10F | GL_RGB | GL_FLOAT |
| `kDepth24Stencil8` | GL_DEPTH24_STENCIL8 | GL_DEPTH_STENCIL | GL_UNSIGNED_INT_24_8 |

## Key Design Decisions

**`BindAsOutput` is format-aware:** The method checks the texture's own format to decide between `GL_COLOR_ATTACHMENT0 + index` and `GL_DEPTH_STENCIL_ATTACHMENT`. This keeps the group's construction loop uniform — it calls `BindAsOutput(i)` on every target including the depth one (passing 0 as the ignored index).

**`BindForWriting` always re-issues `glDrawBuffers`:** Storing a fixed draw-buffer array in the constructor and re-issuing it every `BindForWriting` call is cheap and safe even if GL state is clobbered between frames.

**`SetStencilOp` uses `glStencilOpSeparate`:** This maps directly to the per-face requirement (front vs back) needed by the two-pass light volume technique.

## Next

Issue #80 — Material color properties (ambient, diffuse, emissive, shininess).

## Skills and Guidelines Used

- `src/CLAUDE.md` — one class per file, Google C++ style, include root = `src/`
- `impl-issue` skill — git workflow, cpplint, conventional commit, PR to dev
