# Sky starts at noon instead of midnight

## Problem

The sky appeared yellow/warm throughout the daytime. The Preetham sky model is
physically correct and produces the expected blue sky at noon — but `WorldTime`
was unconditionally initialized at midnight (hour 0), so the sky always began
at night. With any significant `time_scale`, the first "daylight" period visible
to the user was sunrise/early-morning (warm orange-yellow), never the blue midday
sky.

## Root cause

`WorldTime::WorldTime(float time_scale)` initialised `world_time_` to `0.f`
(midnight, 00:00). `EnvironmentDesc` had no field for a configurable starting
time. Both the game loader and the editor's `EnvironmentEditorPanel::EnableSky`
called `WorldTime(env.time_scale)`, always starting at midnight.

## Changes

### `src/environment/EnvironmentDesc.h`
Added `float start_time_of_day = 12.f` (noon by default).

### `src/environment/WorldTime.h` / `WorldTime.cpp`
Extended constructor signature to `WorldTime(float time_scale, float start_time)`.
`world_time_` is now initialised as `start_time * 3600.f`.

### `src/game/MapLoader.cpp`
Parses the new optional `start_time_of_day` YAML key.

### `src/editor/MapSerializer.cpp`
Writes `start_time_of_day` when it differs from the default (12.0).

### `src/editor/EnvironmentEditorPanel.cpp`
- `EnableSky` passes `desc.start_time_of_day` to the `WorldTime` constructor.
- `RenderTimeSection` adds a "Start time" slider (0–24 h) so the initial
  time of day can be edited and saved from within the editor.

## Decisions

- Default of **12.0 h (noon)** chosen because it immediately shows the sky at
  its clearest blue state, giving useful visual feedback as soon as sky is
  toggled on in the editor.
- The field is serialised only when non-default (same pattern as all other
  optional environment fields), so existing map files are unaffected.
- `WorldTime::SetTimeOfDay(float hours)` already exists and is still exposed
  for runtime adjustment (editor slider + game code).

## Notes for future work

- The editor "Start time" slider changes `env.start_time_of_day` (saved to
  YAML) but does **not** restart the running `WorldTime`; the change takes
  effect only on the next map load / sky toggle. If live reset is desired,
  add `world_time_->SetTimeOfDay(env.start_time_of_day)` in response to that
  slider interaction.
- Skills used: none (direct analysis and edit).
- Relevant CLAUDE.md rules applied: one class per file, no superfluous
  comments, cpplint clean.
