# Speed-Sensitive Steering

## Issue

[#825](https://github.com/seblef/claudeengine/issues/825) — at high speed, turning caused the car to lose grip
abruptly. `max_steer_angle` was a single fixed constant baked into Jolt's `WheelSettingsWV` at vehicle creation
time and never adjusted afterwards, so full-lock steer angles remained available at any speed, pushing the tire
slip angle past the friction curve's peak.

## Changes

- `src/physics/VehicleDesc.h`: added two Jolt-free POD fields to `VehicleDesc` — `min_steer_scale` (default
  `0.4f`) and `high_speed_reference_speed` (default `25.f` m/s).
- `src/game/GameVehicle.cpp`: added an anonymous-namespace helper `ComputeSteerScale(speed, desc)` that linearly
  ramps the steer multiplier from `1.0` at 0 m/s down to `desc.min_steer_scale` at
  `desc.high_speed_reference_speed`, using `std::clamp` + `std::lerp`. Applied it to the single `SetSteer()` call
  site in `GameVehicle::Update`, so every controller (player and AI) is scaled uniformly.
- `src/game/VehicleTemplate.cpp`: parse `min_steer_scale` and `high_speed_reference_speed` from the `physics:`
  YAML section, alongside `max_steer_angle`.
- `src/editor/VehicleEditorWindow.cpp`: parse/serialize the two new keys, and expose them as two `ImGui::SliderFloat`
  controls in `DrawPhysicsSection()`, next to "Max steer angle (deg)".

## Rationale

This is the standard arcade-driving-game fix: reduce steering authority as speed increases rather than touching
Jolt's internal tire friction model (`mLateralFriction` / `mLongitudinalFriction` on `WheelSettingsWV`), which
would be a separate, more invasive change to the grip model itself. A single linear ramp was judged sufficient
for a first pass — no need for a nonlinear response curve or per-wheel-pair (front/rear) tuning, since steering
is currently front-only (`is_steered`) for all existing vehicle data. No AI-controller-specific tuning was added:
the fix lives in the shared `GameVehicle::Update`, by design, so all controllers benefit uniformly.

## Follow-up for next features

- No `data/vehicles/*.vehicle.yaml` files were tuned with non-default values — the issue marked this optional,
  pending in-game feel validation. Defaults (`min_steer_scale = 0.4`, `high_speed_reference_speed = 25 m/s`)
  apply to every vehicle until then.
- If the linear ramp doesn't feel right once tested in-game, a curve-editor-driven nonlinear response was
  explicitly called out as a follow-up in the issue, not implemented here.

## Skills / instructions followed

- No skill files were invoked for this task (direct issue implementation via `/impl-issue`).
- Followed `src/CLAUDE.md` (Google style, one class per file, project-relative includes — N/A here since only
  free functions/fields were added, no new files) and `src/physics/CLAUDE.md` (`VehicleDesc` stays Jolt-free POD).
- Followed the root `CLAUDE.md` git workflow: branched from latest `dev`, ran `cpplint`, conventional commit,
  PR back to `dev` with closing keyword.
