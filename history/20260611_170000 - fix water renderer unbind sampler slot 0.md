# fix(environment): unbind sampler slot 0 after water pass

## Changes

- **`src/environment/WaterRenderer.cpp`**: Added `video_->UnbindSampler(0)` in `WaterRenderer::Render()` alongside the existing unbind calls for slots 1, 2, 3, and `kSsrSlot` (4) at the end of Pass 2.

## Decisions & Rationale

The fix is minimal and mechanical: slot 0 holds `normal_map_tex_` (the procedural water normal map) and was the only slot not explicitly unbound after the water pass. The composite pass currently overwrites slot 0 before it is read again, so there was no visible artifact — but the inconsistency is a latent hazard for any future pass ordering change.

## Output to keep in mind

- All sampler slots (0–4) are now consistently unbound after `WaterRenderer::Render()`.
- Pattern established: whenever binding a texture to a sampler slot, always pair it with an `UnbindSampler` call after the draw.

## Skills & instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style guide, project-relative includes
- `src/environment/CLAUDE.md`: one class per file, no platform/OpenGL headers
- Root `CLAUDE.md`: git workflow (checkout dev → branch → implement → cpplint → commit → PR to dev)
