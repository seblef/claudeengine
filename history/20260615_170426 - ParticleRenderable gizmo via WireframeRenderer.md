# ParticleRenderable Gizmo via WireframeRenderer — Issue #527

## Summary

Wired `ParticleRenderable::Enqueue()` into the unified `WireframeRenderer`
gizmo path so that particle system radius indicators are pushed as overlay
spheres during the visibility pass, fully frustum-culled, and no longer
require a separate editor-side `ParticleGizmoRenderer`.

## Changes

### `renderer/ParticleRenderable.h`
- Added `SetGizmoKey(const void* key)` public setter.
- Added `const void* gizmo_key_ = nullptr` private member with
  `cppcheck-suppress unusedStructMember` annotation (matching the pattern
  used by `Light`).

### `renderer/ParticleRenderable.cpp`
- Added `#include "renderer/WireframeRenderer.h"`.
- Added three anonymous-namespace constants:
  - `kDefaultRadius = 0.5f` — sphere radius for point emitters.
  - `kGizmoCyan(0.2f, 0.8f, 1.0f, 1.0f)` — normal gizmo colour.
  - `kHighlightCyan(0.5f, 1.0f, 1.0f, 1.0f)` — selected/highlighted colour.
- In `Enqueue()`, after the `ParticleRenderer` enqueue block, calls
  `WireframeRenderer::Instance().PushOverlaySphere()` when gizmos are
  enabled. Radius falls back to `kDefaultRadius` for `kPoint` emitters
  (which have no meaningful `emitter_radius`); all other shapes use
  `desc.emitter_radius`. Colour is chosen via `IsHighlighted(gizmo_key_)`.

### `game/GameParticleSystem.cpp`
- In `OnAddedToScene()`, after constructing each `ParticleRenderable`,
  calls `renderable->SetGizmoKey(this)` so that
  `WireframeRenderer::SetHighlightedObject(selected_game_object*)` matches
  the owning `GameParticleSystem` pointer and the sphere is highlighted
  when the particle system is selected.

## Design decisions

- `EmitterShape.h` is not explicitly re-included in `ParticleRenderable.cpp`
  because it is already transitively pulled in via
  `ParticleEmitter.h` → `ParticleSubSystemDesc.h` → `EmitterShape.h`.
- `PushOverlaySphere` (depth-test OFF) is used rather than `PushSphere` so
  the gizmo is always visible, matching the behaviour of the former
  `ParticleGizmoRenderer`.
- Colour constants are file-local (anonymous namespace) rather than header
  constants to avoid ODR issues and keep them out of the public API, same
  approach as other renderer gizmo files.

## What to keep in mind

- `ParticleGizmoRenderer` (editor module) can now be deleted; it is
  superseded by this path. That cleanup is deferred to a follow-up issue.
- The `gizmo_key_` / `SetGizmoKey` / `IsHighlighted` pattern is now used
  by both `Light` (issue #526) and `ParticleRenderable` (this issue). Any
  future renderable needing highlight support should follow the same pattern.
- `kDefaultRadius = 0.5f` matches the value that `ParticleGizmoRenderer`
  used for point emitters.

## Skills / CLAUDE.md sections followed

- `impl-issue` skill (branch from dev, cpplint, conventional commit, PR to dev).
- `src/CLAUDE.md`: one class per file; Google C++ style; `#include` paths
  project-relative from `src/`.
- `src/renderer/` follows the `cppcheck-suppress unusedStructMember` pattern
  for private members only used in `.cpp` files.
