# fix(renderer): GBuffer depth texture feedback loop — #476

## Problem

In `Renderer::Update`, the lighting pass bound the GBuffer depth RT as sampler slot 8:

```cpp
gbuffer_.GetDepthRT()->BindAsSampler(8);
LightRenderer::Instance().Render();
// slot 8 never unbound
emissive_fbo_.BindForWriting();   // emissive FBO uses the same depth texture as attachment
```

Slot 8 was never explicitly unbound after the lighting pass. The emissive FBO shares
`gbuffer_.GetDepthRT()` as its depth attachment. From the sky pass onward, the same
GL texture object was simultaneously bound as a sampler (slot 8) and as the active
FBO's depth attachment — a texture feedback loop per OpenGL 4.6 §9.4.4.

While depth writes are disabled during those passes (reducing practical corruption risk),
some drivers re-validate the feedback loop condition on every draw call, which can
degrade GPU state consistency in a threshold-dependent way as scene complexity grows.

## Fix

One line added in `src/renderer/Renderer.cpp`, immediately after `LightRenderer::Instance().Render()`:

```cpp
video_->UnbindSampler(8);
```

This ensures slot 8 is clear before `emissive_fbo_.BindForWriting()` is called.

## Decisions

- Unbind at the earliest safe point (right after the render call, before blend state is reset and FBO unbound) to keep the window of potential feedback as short as possible.
- No changes to `LightRenderer` internals — the unbind belongs at the call site that owns the sampler binding lifecycle.

## Output / notes for next features

- `video_->UnbindSampler(N)` is the correct call to release a sampler slot from the abstract video device.
- The pattern `BindAsSampler` / `UnbindSampler` should always be paired at the same scope level. Consider auditing other `BindAsSampler` calls to ensure they are all unbounded before FBO rebinds.
- Skills used: `impl-issue`
- CLAUDE.md rules applied: conventional commit format, history file required, branch from `dev`.
