# fix(renderer): unbind GBuffer and shadow-map sampler slots after lighting pass

## Changes

### `src/renderer/Renderer.cpp`
Added `UnbindSampler(5)`, `UnbindSampler(6)`, `UnbindSampler(7)` after `LightRenderer::Instance().Render()` returns, alongside the pre-existing `UnbindSampler(8)`. This cleans up all four GBuffer sampler slots (albedo=5, normal=6, specular=7, depth=8) bound by `gbuffer_.BindForReading(5)` and `gbuffer_.GetDepthRT()->BindAsSampler(8)`.

### `src/renderer/LightRenderer.cpp`
Two additions:

1. **`RenderGlobalLights`**: Added a loop `for (int i = 0; i < kCSMCascadeCount; ++i) video_->UnbindSampler(9 + i);` after the global light draw loop to release the CSM cascade shadow maps bound at slots 9–12.

2. **`RenderLocalLights`**: Added per-light unbind after sub-pass B's `RenderIndexed` call:
   - Slot 9 unbound for `kCircleSpot` / `kRectSpot` (spot-light shadow map)
   - Slot 13 unbound for `kOmni` (omni cube shadow map)

## Decisions & Rationale

- The issue requested unbinding slots 9–13 "at the end of each light's draw" rather than after the full `RenderLocalLights` loop. This is preferable because it keeps the shadow slot clean between light iterations, preventing an early-out or future refactor from leaving a stale binding.
- `UnbindSampler` for slots where no shadow map was bound (e.g., shadow-casting disabled) is safe — it simply clears whatever texture was previously there, which is the desired outcome.
- The CSM unbinds are placed after the global light loop because all global lights share the same CSM cascade bindings; unbinding once after all draws is efficient.

## Output to keep in mind

- After this fix, the full set of sampler slots used by the lighting pass (0–13) are explicitly cleaned up before subsequent passes execute.
- Pattern for future passes: bind → draw → unbind should be symmetric. Any new pass that binds a sampler should have a matching unbind before returning.

## Skills & instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style guide, project-relative includes
- `src/renderer` architecture (GBuffer, LightRenderer, ShadowRenderer, CSM)
- Root `CLAUDE.md`: git workflow (checkout dev → branch → implement → cpplint → commit → PR to dev)
