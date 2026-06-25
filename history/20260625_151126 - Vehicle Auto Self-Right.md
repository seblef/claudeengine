# Vehicle Auto Self-Right (#788)

## Summary

Implements automatic self-righting for vehicles that come to rest upside-down, with a 2-second delay and a 1.5-second cosmetic flicker after the snap.

## Files changed

| File | Change |
|---|---|
| `src/physics/PhysicsVehicle.h/.cpp` | Added `SetBodyTransform(const core::Mat4f&)` and `ZeroVelocities()` |
| `src/game/GameVehicle.h` | Extended `DriveState` with `kFlipped` / `kRecovering`; added `flip_timer_`, `recovery_timer_`, `SelfRight()`, `SetMeshesVisible()` |
| `src/game/GameVehicle.cpp` | Detection logic, state machine, `SelfRight()`, flicker, input suppression |
| `src/game/GameMesh.h/.cpp` | Added `SetVisible(bool)` / `IsVisible()` with `visible_` flag |

## Decisions and rationale

### Matrix convention (column 1 = vehicle up)

The issue specification used `transform[1][0..2]` to extract the vehicle's up direction. After analysing `JoltMatToMat4f` and the `Vec3f * Mat4f` operator (which uses column-vector semantics, `result[i] = Σ M[i][j] * v[j]`), the correct extraction for the vehicle's local Y axis in world space is **column 1** of the transform: `(transform(0,1), transform(1,1), transform(2,1))`. This is consistent with a standard local-to-world model matrix where column j holds local axis j in world space.

### Yaw extraction in SelfRight

For `core::Mat4f::RotationY(yaw)`, column 2 (the forward axis) is `(sin(yaw), 0, cos(yaw))`. Given the projected forward `(fx, 0, fz)` from column 2 of the current body transform, recovering yaw is: `atan2(fx, fz)`. This is `atan2(sin, cos)` which is the standard convention for `atan2`.

### SetBodyTransform API

The issue specified `SetBodyTransform(const core::Vec3f& position, const core::Quatf& rotation)` but the codebase has no `core::Quatf`. The API was adapted to `SetBodyTransform(const core::Mat4f& transform)` which:
- Extracts position from column 3
- Builds a Jolt `Mat44` from the upper-left 3×3 and calls `GetQuaternion()` to convert to `JPH::Quat` internally
- Aligns with the existing `GetBodyWorldTransform()` return type

### Mesh visibility (AddRenderable / RemoveRenderable)

The renderer uses a persistent registration model: `AddRenderable` / `RemoveRenderable` control the presence in the visibility system. `SetVisible(bool)` in `GameMesh` calls the appropriate method when in scene. `OnAddedToScene` / `OnRemovedFromScene` were updated to respect the `visible_` flag, so toggling visibility is safe at any lifecycle phase.

### Input suppression

Inputs are suppressed in `kFlipped` (vehicle is stuck, inputs are meaningless and would confuse the state machine). During `kRecovering` inputs are also suppressed to give the physics a moment to settle after the snap; this extends slightly beyond the issue spec which only mentioned `kFlipped`, but is the safer UX choice.

## Keep in mind for next features

- `DriveState` now has five values. Any new code inspecting or switching over `DriveState` must handle `kFlipped` and `kRecovering`.
- `GameMesh::SetVisible` does not affect the physics body — the body continues to simulate even when invisible (intentional: no invulnerability during flicker).
- The recovery flicker frequency (`kFlickerFrequency`) and delay (`kFlipDelay`) are file-scope constants in `GameVehicle.cpp`. If the designer wants these tunable at runtime they would need to be moved to `VehicleDesc` / a YAML config.

## Skills and instructions used

- `/impl-issue` skill
- `src/CLAUDE.md`: one class per `.h/.cpp` pair, include root is `src/`, Google C++ style
- `src/game/CLAUDE.md`: `AddRenderable/RemoveRenderable` lifecycle, `OnAddedToScene/OnRemovedFromScene` pattern
- `src/physics/CLAUDE.md`: Jolt types must not appear in `physics/*.h`; public headers expose only engine-native types
- `CLAUDE.md` (project): dev workflow (branch from `dev`, cpplint, PR to `dev`)
