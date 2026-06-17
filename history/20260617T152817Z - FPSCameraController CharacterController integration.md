# FPSCameraController — CharacterController Integration

**Issue**: #568  
**Branch**: `feat/fps-camera-character-controller-568`  
**Date**: 2026-06-17

---

## Summary

Replaced the raw position integration in `FPSCameraController` with a physics-driven
`CharacterController` (Jolt `CharacterVirtual` capsule).  Mouse-look (yaw/pitch) is
unchanged; only the movement path was reworked.

---

## Changes

### `src/game/FPSCameraController.h`

- Added `#include <memory>` and a forward-declaration of `physics::CharacterController`.
- Removed the `terrain::TerrainData*` member and `SetTerrain()` declaration (terrain
  collision is now handled by the static physics body registered in issue #567).
- Added `std::unique_ptr<physics::CharacterController> character_` member.
- Added `~FPSCameraController()` destructor declaration.
- Updated constants: `kPlayerHeight` → `kCapsuleRadius`, `kCapsuleHalfHeight`,
  `kEyeOffset`; `kGravity` reduced from 18 m/s² to 9.81 m/s² (standard gravity —
  Jolt applies its own gravity inside `CharacterVirtual::Update()`; we only apply
  the vertical component externally when airborne).
- Updated class doc-comment.

### `src/game/FPSCameraController.cpp`

- Added includes: `physics/CharacterController.h`, `physics/PhysicsSystem.h`.
- Removed: `terrain/TerrainData.h` include.
- Added destructor: calls `character_.reset()`.
- `SetCamera()`: if `PhysicsSystem::IsInstanced()`, creates the capsule at
  `position_` via `PhysicsSystem::Instance().CreateCharacter(...)`.
- `Update()`:
  - Computes horizontal velocity (`hvel`) from WASD; flattens Y so look-direction
    slope does not add unwanted vertical motion.
  - **Physics path** (`character_` valid):
    - Grounded: `vel_y_` = 0 (or `kJumpSpeed` on Space).
    - Airborne: `vel_y_ -= kGravity * dt`.
    - Calls `character_->Move({hvel.x, vel_y_, hvel.z}, dt)`.
    - Reads capsule base position from `character_->GetWorldTransform()`, adds
      `kCapsuleHalfHeight + kEyeOffset` for the eye position.
  - **Free-fly fallback** (no `PhysicsSystem`): unchanged behaviour — arrow keys +
    A/Z free movement using `position_` directly.

### `src/app/main.cpp`

- Removed the `controller.SetTerrain(...)` call (the method no longer exists).

---

## Decisions

| Decision | Rationale |
|---|---|
| Keep free-fly fallback | Allows the engine to run without a `PhysicsSystem` (unit tests, lightweight tools). |
| `vel_y_` shared across both paths | Avoids duplicating jump/gravity state; clearly owned by the controller. |
| Gravity = 9.81 m/s² (not 18) | With Jolt, gravity is applied to the capsule body's linear velocity inside `CharacterVirtual::Update()` using the world gravity vector. The external `vel_y_` only supplements the vertical intent signal. Using 18 previously compensated for missing physics; now it would over-accelerate. |
| `hvel.y = 0` after building horizontal velocity | Camera `look` vector has a pitch component; without zeroing Y, moving forward on a slope adds unintended vertical velocity to the physics capsule. |
| Removed `grounded_` bool member | Replaced by `character_->IsGrounded()` (authoritative Jolt result) in the physics path; the free-fly path never needed a proper grounded state. |

---

## Output to keep in mind for next features

- `FPSCameraController` no longer depends on `TerrainData`. If other callers (e.g., an
  editor camera) need terrain height-clamping, they must implement it separately.
- The eye offset above the capsule base is `kCapsuleHalfHeight + kEyeOffset` =
  `0.9 + 0.15 = 1.05 m`. Total standing eye height from ground ≈ 2.2 m (including the
  bottom hemisphere of radius 0.4 m).
- Jump impulse (`kJumpSpeed = 7 m/s`) feeds `vel_y_`; Jolt resolves the actual
  displacement each frame. Tuning feel means adjusting `kJumpSpeed` and/or `kGravity`.

---

## Skills and CLAUDE.md instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h/.cpp` pair; include root is `src/`; Google C++ style.
- `src/game/CLAUDE.md`: dependency graph (`game → physics → core`); one class per file.
- `src/physics/CLAUDE.md`: Jolt types must not appear in `physics/*.h` consumed by
  `game/`; forward-declarations only.

---

## Proposed CLAUDE.md improvements

- `src/game/CLAUDE.md`: update `FPSCameraController` row to note "physics-driven
  movement via `CharacterController` when `PhysicsSystem` is present; free-fly fallback
  otherwise."
- `src/game/CLAUDE.md`: add a row for the dependency on `physics/` (game already
  depends on it for `PhysicsSystem`; making it explicit avoids confusion).
