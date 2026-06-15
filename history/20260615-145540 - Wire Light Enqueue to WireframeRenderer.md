# Wire Light::Enqueue() to WireframeRenderer; add gizmo key to GameLight (#526)

## Summary

Issue #526 (milestone: WireframeRenderer — unified debug draw). Wires light
wireframe gizmo drawing into the renderer's visibility pass via
`Light::Enqueue()`, so that light shapes are frustum-culled for free and the
editor layer no longer needs to iterate scene objects for this purpose.

## Changes

### `renderer/Light.h`
- Added public `SetGizmoKey(const void* key)` — stores an opaque pointer used
  by `WireframeRenderer::IsHighlighted()` for selection highlight without
  coupling the renderer to the game layer.
- Added private `const void* gizmo_key_ = nullptr` member (with
  `cppcheck-suppress unusedStructMember`).
- Added private `void EnqueueWireframe(const core::Color& color)` declaration.

### `renderer/Light.cpp`
- Added includes: `<cmath>`, `core/Vec3f.h`, `renderer/CircleSpotLight.h`,
  `renderer/OmniLight.h`, `renderer/RectangleSpotLight.h`,
  `renderer/WireframeRenderer.h`.
- Added anonymous-namespace constant `kHighlightBlue(0.f, 0.53f, 1.f, 1.f)`
  matching the existing `kColorSelected` in `LightWireframeRenderer`.
- Updated `Enqueue()`: after `LightRenderer::Instance().AddLight(this)`, calls
  `EnqueueWireframe()` when gizmos are enabled and the light is not `kGlobal`.
  Color is `kHighlightBlue` if highlighted, otherwise the light's own `color_`.
- Implemented `EnqueueWireframe()` with a `switch` on `type_`:
  - **kOmni**: `PushSphere(radius, color, Translation(pos))`.
  - **kCircleSpot**: Gram-Schmidt basis (right/fwd from direction), builds a
    rotation+translation `Mat4f` mapping local +Z to the spot direction, calls
    `PushCone(outer_angle, range, color, transform)`.
  - **kRectSpot**: Computes 4 world-space base corners identical to
    `LightWireframeRenderer::BuildVertices()`, calls `PushSegment` 8 times with
    identity transform (coordinates already world-space).

### `game/GameLight.cpp`
- Added `light_->SetGizmoKey(this)` in the `GameLight` constructor, bridging
  the `game::GameLight*` to the renderer-side pointer comparison without
  coupling the renderer to the game module.

## Decisions

**Why use `color_` for non-highlighted, not a fixed yellow?**
The issue specification explicitly passes the light's own color when not
highlighted. This is a design change from the old `LightWireframeRenderer`
(which always used yellow). The light's color provides a more informative gizmo.

**Why does `LightWireframeRenderer` stay?**
It is not removed in this issue. Removing it is a follow-up task that should
happen once the editor's `EditorScene` switches to relying on
`WireframeRenderer` for light gizmos. This issue only adds the push path in
`Enqueue()`.

**Why identity transform for kRectSpot segments?**
All corner positions are computed in world space, so the identity transform is
correct. Using a rotation matrix and local coordinates would have required more
complex math with no benefit.

**Circular include concern**
Including `OmniLight.h`, `CircleSpotLight.h`, `RectangleSpotLight.h` from
`Light.cpp` (the base class's translation unit) is safe because the `.h` guards
prevent re-inclusion, and this pattern is standard for type-switch dispatch on
subclasses from the base class implementation.

## Output to keep in mind

- `gizmo_key_` is set to `this` (the `game::GameLight*`) in the game layer and
  compared via `WireframeRenderer::SetHighlightedObject(key)` / `IsHighlighted`.
  The editor must call `WireframeRenderer::Instance().SetHighlightedObject(selected_game_light)`
  to drive selection highlight — this wiring is an editor-side task.
- The old `LightWireframeRenderer` in the editor still runs in parallel; once
  the editor is updated to rely on `WireframeRenderer`, `LightWireframeRenderer`
  can be removed.
- `kHighlightBlue` is defined in the anonymous namespace of `Light.cpp` so it
  is not part of any public API.

## Skills / instructions followed
- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h`/`.cpp`, include root is `src/`, Google
  C++ style, conventional commits, history file required.
- `src/game/CLAUDE.md`: `game` must not be imported by `renderer`; gizmo key
  is an opaque pointer to avoid that coupling.
- `src/renderer/CLAUDE.md` (CLAUDE.md): protected files not deleted.
- Global `CLAUDE.md`: feature branch from `dev`, cpplint, conventional commit,
  PR to `dev`.
