# ChaseCameraController — Issue #714

**Date:** 2026-06-22  
**Branch:** `feat/chase-camera-controller-714`  
**PR:** [#736](https://github.com/seblef/claudeengine/pull/736)  
**Milestone:** M2 — Driveable Car

---

## What was done

Added `game/ChaseCameraController.h` and `game/ChaseCameraController.cpp`, implementing a third-person spring-arm camera controller as an `ICameraController` subclass.

Registered the new `.cpp` in `src/game/CMakeLists.txt`.

---

## Design decisions

### Layer mask for raycast

The issue specification mentions `kLayerWorld | kLayerStatic`, but `kLayerStatic` does not exist in `physics/CollisionLayer.h`. Only `kLayerWorld` covers static world geometry (terrain and static meshes). The wall-clip raycast uses `(1u << physics::kLayerWorld)` — a single-bit bitmask matching the convention in `PhysicsSystem.cpp`'s `RaycastObjectLayerFilter`.

### Look direction is unsmoothed

Per the issue notes: smoothing the look direction independently of position causes the camera to momentarily point the wrong way when the vehicle turns sharply. The look-at recomputes a fresh forward/right/up basis every frame from the current (smoothed) position to the target.

### First-frame snap

When `initialized_` is false, the camera teleports directly to the clipped arm position and sets `initialized_ = true`. Subsequent frames apply the spring factor. This prevents a visible initial jump from `Vec3f::kZero` to the actual starting position.

### PhysicsSystem guard

The raycast is wrapped in `physics::PhysicsSystem::IsInstanced()` following the same pattern used by `FPSCameraController`'s `CharacterController` creation. This allows the controller to run in unit tests without a physics world.

### Matrix column convention

Extracted from `GameCamera.cpp` and `FPSCameraController.cpp`:
- Column 0 = right
- Column 1 = up
- Column 2 = −forward (backward in right-handed −Z convention)
- Column 3 = position

---

## Files touched

| File | Change |
|---|---|
| `src/game/ChaseCameraController.h` | New — class declaration |
| `src/game/ChaseCameraController.cpp` | New — implementation |
| `src/game/CMakeLists.txt` | Added `ChaseCameraController.cpp` to the `game` static library |

---

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per `.h`/`.cpp` pair, include root is `src/`
- `src/game/CLAUDE.md` — dependency graph, module table, ICameraController pattern
- `src/physics/CLAUDE.md` — Jolt-free public API, layer bitmask convention
- `CLAUDE.md` (root) — cpplint, conventional commits, history file requirement

---

## Output to keep in mind

- `kLayerWorld` is index `0`; the Raycast `layer_mask` is a bitmask, so use `(1u << kLayerWorld)`, not the index directly.
- `ChaseCameraController` targets a `const GameObject*` (not `GameVehicle*`) so it can be reused for other entity types in future milestones.
- The `position_` field is the **smoothed** camera position, not the desired arm position. Only use it for the final transform, not as input to the spring computation.
