# WireframeRenderer — Cylinder and Capsule Primitives

**Issue**: #561  
**Branch**: feat/wireframe-cylinder-capsule-561  
**PR**: to `dev`

## Summary

Extended `renderer/WireframeRenderer` with cylinder and capsule wireframe primitives required by the physics gizmo renderer. Follows the established pattern: private `Append*` static helper → public `Push*` (depth-tested) + `PushOverlay*` twin (overlay, no depth test).

## Changes

### `src/renderer/WireframeRenderer.h`

- Updated class doc comment to list the new primitives.
- 4 new public methods: `PushCylinder`, `PushCapsule`, `PushOverlayCylinder`, `PushOverlayCapsule`.
- 2 new private static helpers: `AppendCylinder`, `AppendCapsule`.
- Added `constexpr float kPi = kTwoPi * 0.5f` to the anonymous namespace.

### `src/renderer/WireframeRenderer.cpp`

- **`AppendCylinder`**: two XZ-plane circles at `y = ±half_height` (using `kAxisX`/`kAxisZ` as the circle axes, offset manually to match the cone pattern) plus 4 vertical edges at `kAxisX`, `kAxisZ`, `-kAxisX`, `-kAxisZ`.
- **`AppendCapsule`**: delegates cylinder body to `AppendCylinder`, then draws 4 half-arcs (XY and ZY planes, one per hemisphere) by iterating `kCircleSegments/2 = 16` steps over `[0, π]` for the top cap and `[π, 2π]` for the bottom cap.

## Decisions

- **Circle axes for cylinder**: XZ plane (axes `kAxisX` + `kAxisZ`), matching the issue spec (Y-up cylinder axis). The existing `AppendCircle` uses axes for the plane, not the axis of rotation, so XZ gives a horizontal circle which is correct.
- **Hemisphere arcs via angle shift**: Rather than a separate `AppendArc` helper, the bottom hemisphere iterates the same loop with `t + kPi`, keeping the implementation concise and consistent with how `AppendCone` handles offsets manually.
- **No new `AppendArc` helper**: The arcs are short enough (16 segments each) that inlining the loop body avoids over-abstraction. The capsule is the only consumer.
- **`kPi` constant**: Added to the anonymous namespace alongside `kTwoPi` for clarity; avoids a dependency on `M_PI` or a magic literal.

## Output to remember

- The cylinder uses **XZ** as the circle plane (Y is the axis): pass `kAxisX` and `kAxisZ` as the two radial directions.
- The capsule's hemisphere arcs produce 4 × 16 = 64 segments per hemisphere on top of the 2 × 32 + 4 = 68 segments from the cylinder body — 196 segments total per capsule.
- The `PushOverlay*` twins push to `overlay_list_` for depth-test-OFF rendering, consistent with all existing overlay helpers.

## Skills and CLAUDE.md instructions used

- `impl-issue` skill for the overall workflow.
- Root `CLAUDE.md`: conventional commits, history file, cpplint before commit.
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`.
