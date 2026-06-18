# Fix: Player Does Not Start at Player-Start Entity Position

**Issue**: #650  
**Branch**: `fix/player-start-position-650`  
**Date**: 2026-06-18

---

## Problem

After loading a map in the game app, the player did not start at the position
defined by the `player_start` entity. Instead the player was displaced — typically
landing on the terrain surface below the entity position rather than at the entity's
world-space location.

---

## Root Cause

`GameSystem` is constructed and captures `prev_time_` (via
`std::chrono::steady_clock::now()`) **before** map loading begins.  Map loading is
not instantaneous: parsing YAML, building physics shapes for terrain and meshes,
uploading textures, and populating the broadphase can take several seconds.

When the main loop calls `game.Update()` for the very first time, the computed
`dt = now - prev_time_` therefore includes all of that loading time — potentially
2–5 seconds on complex maps.

`FPSCameraController::Update()` uses this inflated `dt` on its first tick:

1. The `CharacterController` (Jolt `CharacterVirtual` capsule) is just created at
   `position_` — the correct player-start world position.
2. `IsGrounded()` returns `false` on a freshly created character (no contact has
   been detected yet).
3. The airborne branch applies `vel_y_ -= gravity_ * dt` — with `dt` equal to the
   loading time, this accumulates a very large downward velocity.
4. `character_->Move({0, vel_y_, 0}, dt)` is called: Jolt simulates many seconds of
   free-fall in one step and, via CCD, stops the capsule at the terrain surface.
5. `GetWorldTransform()` now returns the *terrain-surface* position, not the
   player-start entity position.

If the terrain height at the player-start XZ happens to equal `player_start.y`,
the error is imperceptible; when it differs (even by a metre), the player visibly
spawns at the wrong height.

---

## Fix

Added `GameSystem::ResetTimer()` which resets `prev_time_` to
`std::chrono::steady_clock::now()`.  A single call in `main.cpp` — placed
immediately before the main `while` loop, after all setup (map loading, object
placement, camera and controller wiring) — guarantees that the first `dt` is a
genuine frame time (a few milliseconds) rather than the entire loading duration.

### Files changed

| File | Change |
|---|---|
| `src/game/GameSystem.h` | Added `void ResetTimer()` declaration with doc-comment |
| `src/game/GameSystem.cpp` | Implemented `ResetTimer()` (one-liner: resets `prev_time_`) |
| `src/app/main.cpp` | Called `game.ResetTimer()` just before the main loop |

---

## Decisions

| Decision | Rationale |
|---|---|
| `ResetTimer()` instead of capping dt | A max-dt cap hides the symptom everywhere; resetting the timer before the loop is explicit and fixes the root cause (setup-time inflation) without affecting steady-state frame pacing or any pathological long frames during gameplay. |
| Public API rather than inline fix | `GameSystem` already owns `prev_time_`; exposing the reset as a named method documents the intent and keeps `main.cpp` from reaching into internals. |
| No change to `FPSCameraController` | The controller logic is correct; the bug is in the timer, not the movement math. |

---

## Output to Keep in Mind

- Any future code path that creates `GameSystem` long before the main loop starts
  (e.g., a map-reload feature, a loading screen, or an async asset pipeline) must
  call `ResetTimer()` again after setup is complete.
- `ResetTimer()` does not reset `elapsed_time_`; callers that track game time since
  session start will continue to accumulate time across resets.

---

## Skills and CLAUDE.md Instructions Used

- `impl-issue` skill
- `CLAUDE.md` (root): conventional commits, cpplint before commit, history file,
  PR to `dev` with issue close command.
- `src/CLAUDE.md`: Google C++ style guide; one class per `.h/.cpp` pair; include
  paths project-relative from `src/`.
- `src/game/CLAUDE.md`: dependency graph (`app → game`); no platform/OpenGL headers
  in `game/`.
