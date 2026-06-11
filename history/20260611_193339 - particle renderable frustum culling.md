# feat(particles): ParticleRenderable — frustum culling for particle emitters

## Summary

Particle emitters were previously rendered unconditionally every frame by
`ParticleRenderer`, which held a permanent list of registered emitters.  They
were not subject to the frustum-culling visibility system that governs mesh
instances and lights.  This change makes each emitter a proper `Renderable`
so it participates in the octree visibility system and is only enqueued when
its world-space AABB intersects the view frustum.

## Changes

### New: `renderer/ParticleRenderable`

A thin `Renderable` subclass in the `renderer` module that wraps a
`particles::ParticleEmitter*`.  No circular dependency is introduced because
`renderer` already depends on `particles`.

- Constructor computes a **conservative local AABB** from the emitter descriptor:
  `r = emitter_radius + speed_max × lifetime_max + half_max_size`
  Y-min is further extended by the gravity fall distance
  (`0.5 × gravity × lifetime_max²`) to prevent premature culling of
  downward-falling particles.
- `Enqueue()` calls `Renderer::Instance().GetParticleRenderer()->EnqueueEmitter()`,
  mirroring the `MeshInstance` → `MeshRenderer` pattern exactly.

### Modified: `particles/ParticleRenderer`

Replaced the permanent `emitters_` list (`Register` / `Unregister`) with a
**per-frame queue** (`frame_emitters_`):

- `BeginFrame()` — clears `frame_emitters_`; called by `Renderer::Update()`
  at the start of each frame (alongside `MeshRenderer::EndRender()`).
- `EnqueueEmitter(emitter)` — adds to `frame_emitters_`; called by
  `ParticleRenderable::Enqueue()` during visibility traversal.
- Both `RenderGeometryPass` and `RenderForwardPass` now iterate
  `frame_emitters_` only.

### Modified: `renderer/Renderer`

- Added `GetParticleRenderer()` accessor (used by `ParticleRenderable::Enqueue()`).
- Added `if (particle_renderer_) particle_renderer_->BeginFrame()` at the start
  of `Update()`, clearing the per-frame queue before the visibility pass.

### Modified: `game/GameParticleSystem`

- `OnAddedToScene()` now creates one `ParticleRenderable` per emitter and
  registers it with `Renderer::AddRenderable()` instead of calling
  `ParticleRenderer::Register()`.
- `OnRemovedFromScene()` calls `Renderer::RemoveRenderable()` for each
  renderable.
- `OnWorldTransformUpdated()` propagates the new world matrix to both emitters
  (for spawn position) and renderables (for culling bbox).
- `Update()` now calls `emitter->UploadToGPU()` after `emitter->Update(dt)` —
  this was a **pre-existing bug**: the GPU vertex buffer was never filled in the
  game path, making particles invisible in-game while working only in the
  particle editor preview.

### Modified: `editor/ParticleEditorWindow`

- `RebuildPreview()` no longer calls `Register`/`Unregister` on the preview
  renderer — emitters are just stored.
- `RenderPreviewFrame()` calls `preview_renderer_->BeginFrame()` and
  `EnqueueEmitter()` for each drawable emitter, then renders.

### Modified: `game/GameSystem`

- Removed `GetParticleRenderer()` (now dead code: `GameParticleSystem` no
  longer routes through `GameSystem` to reach `ParticleRenderer`).

## Decisions

- **Wrapper in `renderer`, not merge**: Keeping `particles` as a separate module
  avoids bloating the renderer with simulation logic.  The dependency direction
  (`renderer → particles`) is unchanged.
- **Per-frame queue, no permanent list**: Matches the `MeshRenderer` pattern and
  ensures only visible emitters (passed through the frustum test) are drawn.
  The `ParticleEditorWindow`'s private `preview_renderer_` is updated to enqueue
  per-frame, which is more explicit and simpler than the previous
  `Register`/`Unregister` API.
- **Conservative AABB**: Slightly over-estimates emitter reach to guarantee no
  false culling.  Gravity-induced downward displacement is added to Y-min only
  to keep the box tight on the upward side.
- **`UploadToGPU()` fix**: Added in `GameParticleSystem::Update()` to fix the
  pre-existing silent bug where game-path particles were simulated but never
  uploaded, making them invisible.

## Output to keep in mind

- `ParticleRenderable` lives in `renderer/`, follows the `MeshInstance` pattern.
- The bbox is static (computed once at construction).  If a particle system has
  extreme velocity or lifetime changes at runtime, the bbox will not adapt —
  that is acceptable given the conservative formula.
- The `ParticleEditorWindow` preview renderer is now fully stateless between
  frames (no permanent emitter list); all state is rebuilt by `BeginFrame()` +
  `EnqueueEmitter()` each frame.

## Skills and instructions used

- `src/CLAUDE.md`: one class per file, include root is `src/`, Google C++ style
- `src/particles/CLAUDE.md`: dependency graph `renderer → particles`; no circular deps
- `src/renderer/CLAUDE.md` (implicit): mirrored `MeshInstance`/`MeshRenderer` pattern
- `src/game/CLAUDE.md`: `OnAddedToScene`/`OnRemovedFromScene` lifecycle invariant
