# Jump mechanic for FPS controller

**Issue**: #318
**Branch**: feat/jump-mechanic-fps-controller-318

## Changes

### `src/game/FPSCameraController.h`
- Added `kJumpSpeed = 7.f` and `kGravity = 18.f` constexpr constants.
- Added held-key flag `k_jump_` (Space key).
- Added `vel_y_` (vertical velocity, m/s) and `grounded_` state for the jump/gravity simulation.
- Updated class doc comment to mention Space = jump and the suppression of A/Z in terrain mode.

### `src/game/FPSCameraController.cpp`
- `OnEvent()`: `kKeyDown`/`kKeyUp` Space → toggles `k_jump_`.
- `Update()`: A/Z free-fly keys are now suppressed when a terrain is bound (they only apply in free-fly mode).
- `Update()`: After horizontal movement, jump impulse is applied (`vel_y_ = kJumpSpeed`) when Space is held and player is grounded. Gravity is integrated every frame (`vel_y_ -= kGravity * dt`). Landing clamps position to terrain height + `kPlayerHeight` and resets `vel_y_` / `grounded_`. When no terrain is bound, `grounded_` stays `true` (jump suppressed, free-fly mode unaffected).

## Decisions

- **Gravity above 9.8 m/s²**: 18 m/s² chosen per the issue spec for a tighter, more game-feel-friendly arc at `kJumpSpeed = 7 m/s`.
- **A/Z suppression in terrain mode**: prevents the player from using free-fly vertical movement to bypass terrain, keeping the two modes mutually exclusive. This matches the issue note.
- **`k_jump_` held-key, not impulse**: Space is read as a held key so that if the player is briefly airborne (e.g., going over a slope) and holds Space they jump again immediately on landing — standard FPS feel.
- **`vel_y_` gravity runs always**: gravity integrates unconditionally (not only when `!grounded_`) so the landing check can use `<=` instead of `<`, matching the issue spec and preventing floating-point edge cases on flat terrain.

## Output to keep in mind

- `kPlayerHeight`, `kJumpSpeed`, `kGravity` are all `constexpr` in the private section — easy to tune.
- The free-fly A/Z keys are now silently disabled in terrain mode; if a future feature needs vertical free-fly with terrain, that guard will need revisiting.
- No new files or CMake changes needed — everything fits in the existing `FPSCameraController` pair.

## Skills / CLAUDE.md rules applied

- `src/CLAUDE.md`: one class per `.h/.cpp` pair, Google C++ style, include root is `src/`.
- `src/game/CLAUDE.md`: do not include platform/OpenGL headers; `GameSystem` routes events to the controller before calling `Update(dt)`.
- Global `CLAUDE.md`: conventional commits, history file, cpplint check before commit.
- `feedback_cppcheck_suppressions.md`: added `// cppcheck-suppress unusedStructMember` to each new private member.
