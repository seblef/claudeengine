# Physics Debug Draw Panel (Issue #625)

## Summary

Added an ImGui panel section to the existing **Rendering Settings** panel that lets
the editor call `PhysicsSystem::DrawDebug()` every frame with user-controlled settings.

## Changes

### `src/editor/RenderingSettingsPanel.h`

- Added `PhysicsDebugBodyMode` enum (`kSelectedOnly` / `kAllBodies`).
- Added four private state members:
  - `physics_debug_body_mode_` (default: `kSelectedOnly`)
  - `physics_debug_constraints_` (default: `false`)
  - `physics_debug_contact_points_` (default: `false`)
  - `physics_debug_broadphase_` (default: `false`)
- Exposed read-only getters for all four.

### `src/editor/RenderingSettingsPanel.cpp`

Added a collapsing `"Physics Debug Draw"` section below the existing
`"Physics shapes"` checkbox.  Inside it:
- Two radio buttons for body mode (**Selected only** / **All bodies**).
- Three checkboxes for optional layers: **Constraints**, **Contact points**,
  **Broadphase tree**.

### `src/editor/EditorViewport.cpp`

After `WireframeRenderer::Render()`, added a block that:
1. Guards against a missing panel, scene, or uninitialised `PhysicsSystem`.
2. Builds `PhysicsDebugDrawSettings` from the panel state.
3. When body mode is `kSelectedOnly`, walks `scene_->GetSelection()`, casts each
   selected `kMesh` object to `GameMesh`, and collects non-null `PhysicsBody*`
   pointers into a local vector passed as `selectedBodies`.
4. Calls `PhysicsSystem::Instance().DrawDebug(debug_settings)`.

## Decisions

**Placed inside `RenderingSettingsPanel`** rather than a new `PhysicsDebugPanel`
because that panel already owns the "Physics shapes" toggle and keeping
physics-related debug controls together is the least disruptive option.

**Did not remove `PhysicsGizmoRenderer`**: that removal is tracked separately in
issue #627.  Both render paths can coexist: the old gizmo path uses
`WireframeRenderer` directly from `PhysicsBodyDesc` data; the new debug-draw path
uses Jolt's built-in renderer which ultimately also drives `WireframeRenderer` via
`JoltDebugRenderer`.

**No `Step()` dependency**: `DrawDebug` reads body state directly from the Jolt
broadphase without requiring a prior `Step()` call, so it is safe to call from the
editor loop (which never calls `Step()`).

**`PhysicsSystem::IsInstanced()` guard**: the singleton may not be constructed in
all configurations; this check avoids a crash on any future build configuration
that omits physics.

## Output / Next steps

- Issue #627 will remove `PhysicsGizmoRenderer` and the old `"Physics shapes"`
  checkbox, leaving only the new `DrawDebug`-based panel.
- Contact-point drawing requires the simulation to be running (`Step()` must be
  called) for the flag to take effect; in the static editor these will never appear.

## Skills & instructions followed

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, include paths relative to `src/`
- `src/editor/CLAUDE.md`: panel is pure UI; no editing logic inline; owned by `EditorSystem`
- `src/physics/CLAUDE.md`: no Jolt types in public headers consumed by `editor/`
