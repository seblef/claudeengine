# Auto-Reverse State Machine

**Date:** 2026-06-24  
**Branch:** `feat/game-auto-reverse`  
**PR:** #780  
**Closes:** #775

---

## What was done

Implemented the standard racing-game auto-reverse behaviour: holding brake while the vehicle is near-stopped causes it to engage reverse gear after a short delay.

### Files changed

| File | Change |
|---|---|
| `src/physics/PhysicsVehicle.h` | Added `GetForwardSpeed()` declaration |
| `src/physics/PhysicsVehicle.cpp` | Implemented `GetForwardSpeed()` using `body_->GetLinearVelocity().Dot(forward)` |
| `src/game/GameVehicle.h` | Added `DriveState` enum, `drive_state_` / `reverse_timer_` members, `IsReversing()` getter |
| `src/game/GameVehicle.cpp` | Added tuning constants; replaced direct input-forwarding with `DriveState` switch |

---

## Key decisions

### Forward axis verification

The issue asked to verify the Jolt forward-axis convention before committing. Confirmed:
- `VehicleConstraintSettings::mForward` defaults to `Vec3(0, 0, 1)` in the Jolt source (`Jolt/Physics/Vehicle/VehicleConstraint.h:32`).
- Vehicle YAML data corroborates this: front wheels at z≈+4, rear wheels at z≈-4.7.
- `GetForwardSpeed()` therefore uses `JPH::Vec3(0.f, 0.f, 1.f)` — not X+.

### cppcheck suppressions

New private members `drive_state_` and `reverse_timer_` in `GameVehicle` both received `// cppcheck-suppress unusedStructMember` to match the convention used for all other members in that class.

### State machine placement

Logic lives in `GameVehicle` (not `PlayerVehicleController` or `PhysicsVehicle`), because it needs simultaneous access to both controller input and physics speed. This keeps `PhysicsVehicle` a pure simulation wrapper and the controller pure input.

---

## Tuning constants

Defined in the anonymous namespace of `GameVehicle.cpp`:

```cpp
constexpr float kReverseEngageDelay    = 0.3f;
constexpr float kReverseSpeedThreshold = 0.3f;
constexpr float kReverseThrottle       = 0.6f;
```

---

## Future consumers of `IsReversing()`

- **HUD**: display "R" gear indicator.
- **ChaseCameraController**: flip or widen the camera arm when reversing.

---

## Skills and CLAUDE.md instructions used

- `impl-issue` skill (branch/cpplint/commit/PR workflow)
- `src/CLAUDE.md`: one class per file, include root is `src/`, Google C++ style guide
- `src/physics/CLAUDE.md`: Jolt types must not appear in public physics headers
- `src/game/CLAUDE.md`: `GameVehicle` is the correct place for game-level drive state
