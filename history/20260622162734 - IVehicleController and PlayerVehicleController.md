# IVehicleController and PlayerVehicleController

**Date:** 2026-06-22
**Issue:** #712
**Branch:** feat/vehicle-controller-712

## Summary

Introduced the `IVehicleController` abstract interface and its first concrete implementation `PlayerVehicleController`, following the existing `ICameraController` / `FPSCameraController` pattern.

## Files changed

| File | Change |
|---|---|
| `src/game/IVehicleController.h` | New abstract interface |
| `src/game/PlayerVehicleController.h` | New concrete class declaration |
| `src/game/PlayerVehicleController.cpp` | New concrete class implementation |
| `src/game/CMakeLists.txt` | Added `PlayerVehicleController.cpp` to the `game` library |

## Design decisions

### Interface shape
`IVehicleController` is a pure abstract class with deleted copy operations and a protected default constructor — the exact same pattern as `ICameraController`. The four accessors (`GetThrottle`, `GetSteer`, `GetBrake`, `GetHandbrake`) are marked `[[nodiscard]]` and `const` to prevent accidental discard and mutation.

### Steer ramping
Steering ramps toward the target (`±1`) at `kSteerSpeed = 2.0 f/s` and returns to 0 at `kSteerReturnSpeed = 3.0 f/s` when no key is held. This gives an analog feel on a digital keyboard and matches the spec exactly. The constants are `static constexpr` class members so they are easy to tune.

### Throttle / brake mutual exclusion
When both throttle (W / Up) and brake (S / Down) are held simultaneously, throttle takes priority and brake is set to 0. This is handled with a single conditional in `Update()` and documented with an inline comment.

### No dependency on any new config
Unlike `FPSCameraController`, which loads tuning from `AppConfig`, the steer rates are hard-coded constants for now. Gamepad / config support is deferred to M7.

## Output to keep in mind

- `IVehicleController` is now the expected interface for `PhysicsVehicle` / `GameVehicle` to consume controller output; wire those types to it rather than direct keyboard polling.
- `AIVehicleController` (M4) must implement `IVehicleController`.
- The `game` CMakeLists only needs `PlayerVehicleController.cpp`; `IVehicleController.h` is header-only.

## Skills used

- `impl-issue`

## Instructions especially taken into account

- `src/CLAUDE.md`: one class per `.h` / `.cpp` pair; Google C++ style; include root is `src/`.
- `src/game/CLAUDE.md`: mirror `ICameraController` pattern; `cppcheck-suppress unusedStructMember` on private members.
- Root `CLAUDE.md` git workflow: checkout `dev`, create `feat/` branch, `cpplint`, conventional commit, PR to `dev`.
