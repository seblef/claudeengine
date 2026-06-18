# Physics debug draw toggle in game app

**Issue**: #626  
**Branch**: feat/physics-debug-draw-app-626  
**Date**: 2026-06-18

## Changes

Single file modified: `src/app/main.cpp`.

### Key binding (P)

Added `bool physics_debug_enabled = false;` alongside the existing `debug_mode` state variable in the debug-mode block. Extended the `SetEventCallback` lambda capture to include `&physics_debug_enabled` and added a toggle on `core::Key::kP`.

### Draw call

Added `physics::PhysicsSystem::Instance().DrawDebug({})` in the main loop, right before `game.Update()`, guarded by `physics_debug_enabled`. Placement rationale: `DrawDebug` pushes wireframes to `WireframeRenderer`; those are flushed during `Renderer::Update()` which is called inside `game.Update()`. Calling it before `game.Update()` ensures the wireframes are rendered in the same frame they are pushed. The physics state used is from the previous frame's `Step()` — a single-frame lag that is standard and imperceptible.

## Decisions

- **No `#ifdef` guard**: `JPH_DEBUG_RENDERER` is enabled for all builds (#622), so none needed.
- **Default settings `{}`**: The issue explicitly requested `DrawDebug({})` (bodies only), consistent with the editor panel's initial state.
- **Call site before `game.Update()`**: The only way to honour "after Step, before Renderer::Update" from `main.cpp` without touching `GameSystem` would require splitting `game.Update()`. Instead, calling just before `game.Update()` ensures correct render-pass coherence (wireframes rendered in the same pass as the scene), accepting the trivial one-frame physics-state lag.

## Output to keep in mind

- The same pattern (`bool flag` + `SetEventCallback` + conditional call before `game.Update()`) can be reused for future per-frame debug overlays without modifying `GameSystem`.
- If a zero-lag draw is required in future, `GameSystem` would need a post-step / pre-render callback hook.

## Skills and instructions used

- `impl-issue` skill
- `CLAUDE.md` (root): git workflow, conventional commits, history file requirement
- `src/CLAUDE.md`: Google C++ style, include paths, one class per file
