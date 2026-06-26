# GamePlayerStart Y-axis Rotation and Direction Arrow

**Issue:** #818  
**Branch:** `feat/player-start-yaw-rotation`  
**Date:** 2026-06-26

---

## Summary

Added Y-axis rotation support to `GamePlayerStart` so level designers can control the spawn direction of the player's vehicle. Three areas were addressed: preserving yaw in `PlacementTool`, drawing a direction arrow in the editor gizmo, and verifying rotation propagation to the physics body.

---

## Changes

### 1 — `src/editor/tools/PlacementTool.cpp`

`UpdatePreviewPosition()` previously rebuilt the world transform from translation only, silently discarding any Y-rotation applied via the `TransformTool` (rotate gizmo).

**Fix:** Extract the current yaw from the object's world transform before overwriting it, then compose `T * Ry`.

```cpp
const core::Mat4f current = preview_object_->GetWorldTransform();
const float fx = current(0, 2), fz = current(2, 2);  // local +Z column
const float fw_len = std::sqrt(fx * fx + fz * fz);
const float yaw = (fw_len > 1e-4f) ? std::atan2(fx, fz) : 0.f;
preview_object_->SetWorldTransform(
    core::Mat4f::Translation({x, hit->y + preview_height_, z}) *
    core::Mat4f::RotationY(yaw));
```

**Yaw extraction rationale:** `Mat4f` is row-major. For `RotationY(yaw)`, column 2 holds `(sin(yaw), 0, cos(yaw))`, so `atan2(m(0,2), m(2,2))` recovers the angle. This is consistent with `GameVehicle::SelfRight()` which uses the same idiom.

**Degenerate case:** When `fw_len ≤ 1e-4f` (identity or near-zero forward, e.g. a brand-new preview object), yaw defaults to 0 — the object faces +Z, which was the existing behaviour.

### 2 — `src/editor/PlayerStartGizmos.cpp`

Added a 3-segment direction arrow along the local +Z axis of each `GamePlayerStart`.

- **Shaft:** `base → tip`, length 1.5 units (fits within the 2-unit pole without cluttering it).
- **Arrowhead:** Two diagonal branches, each 0.3 units back and 0.15 units wide along local +X.
- **Color:** Reuses `flag_color` so the arrow brightens when the object is selected, matching the flag behaviour.

Column vectors are read directly from the (row-major) world transform — no normalization needed for a pure rotation matrix; the flag triangle already relied on this.

```cpp
const core::Vec3f dir(wt(0, 2), wt(1, 2), wt(2, 2));   // local +Z
const core::Vec3f perp(wt(0, 0), wt(1, 0), wt(2, 0));  // local +X
const core::Vec3f tip = base + dir * kArrowLength;
wr.PushOverlaySegment(base, tip, flag_color);
wr.PushOverlaySegment(tip, tip - dir * kArrowHead + perp * kArrowSpread, flag_color);
wr.PushOverlaySegment(tip, tip - dir * kArrowHead - perp * kArrowSpread, flag_color);
```

### 3 — `src/editor/PlayModeManager.cpp` / `src/game/GameVehicle.cpp`

**No changes required.**

`PlayModeManager::Enter()` sets the vehicle's world transform from the player-start transform (line 151), then calls `vehicle_->Activate()` (line 158). `GameVehicle::Activate()` passes `GetWorldTransform()` to `PhysicsSystem::CreateVehicle()`, which calls `ExtractRotation(initial_transform)` — a function that reads the full 3×3 rotation submatrix and converts it to a Jolt quaternion. Rotation is already fully propagated to the physics body on spawn.

---

## Decisions

- **Y-rotation only**: The issue explicitly limits rotation to the Y axis for ground-bound vehicles. The yaw extraction via `atan2(m(0,2), m(2,2))` only recovers the yaw component, so any accidental X/Z tilt on the object is silently stripped — correct behaviour for this constraint.
- **No normalization in gizmo**: Column vectors of a pure rotation matrix are already unit length. Adding normalization would be defensive noise with no benefit.
- **Arrow color follows flag**: Using the same `flag_color` (with selected/unselected branching already computed) avoids introducing a new color constant and keeps the visual language consistent.

---

## Skills and CLAUDE.md rules followed

- `src/CLAUDE.md`: Google C++ style, one class per file, project-relative includes.
- `src/editor/CLAUDE.md`: GUI vs. edition logic separation; `PlacementTool` is an `EditorToolBase` subclass where editing math lives.
- `impl-issue` skill: checkout dev, branch from dev, implement, cpplint, commit, PR.

---

## Notes for future contributions

- The yaw-only constraint in `PlacementTool` means a `GamePlayerStart` placed with a non-zero X or Z tilt will have that tilt zeroed on move. This is acceptable for ground-bound spawns but would need revisiting for flying vehicles.
- The direction arrow in the gizmo always points along local +Z. Vehicle meshes must be authored so their forward direction is +Z for the gizmo to be meaningful.
