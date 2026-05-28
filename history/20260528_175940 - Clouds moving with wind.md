# Clouds Moving with Wind — Issue #341

## Summary

Fixed `EnvironmentEditorPanel::Tick()` so the `WindSystem` is advanced every frame,
allowing the cloud UVs to scroll correctly in the editor.

## Root cause

`CloudRenderer`'s fragment shader scrolls cloud UVs using:

```glsl
vec2 scroll = wind_xz * wind_time * kCloudSpeed;
```

`wind_time` is the accumulated simulation time kept by `WindSystem` and uploaded to
slot 7 via `Renderer::FillWindInfos()`.  In the game (`main.cpp`) `WindSystem::Update(dt)`
is called each frame, so clouds move correctly.  In the editor the previous `Tick()`
implementation was:

```cpp
void EnvironmentEditorPanel::Tick(float dt) {
  if (!world_time_ || time_paused_) return;   // exits when sky is disabled
  world_time_->Advance(dt);
  ...
}
```

Because the early return was gated on `world_time_` (sky), `wind_system_->Update(dt)`
was never called.  `wind_time_` stayed at 0 and clouds were permanently static in the
editor — even when wind was enabled.

## Fix

`src/editor/EnvironmentEditorPanel.cpp` — `Tick()`:

- Separated the `world_time_` guard from the `time_paused_` guard.
- `world_time_` is only advanced when the sky subsystem is active (unchanged behaviour).
- `wind_system_->Update(dt)` is now called whenever the wind subsystem is active,
  independently of whether the sky subsystem is enabled.
- `time_paused_` still stops both world time and wind (consistent: pausing the
  environment simulation halts all time-dependent effects).

## Files changed

| File | Change |
|---|---|
| `src/editor/EnvironmentEditorPanel.cpp` | `Tick()`: advance `wind_system_` each frame |
| `history/20260528_175940 - Clouds moving with wind.md` | This file |

## Notes for future contributions

- The game-mode path (`main.cpp`) already calls `WindSystem::Update()` each frame and
  requires no change.
- Cloud motion speed is controlled by `kCloudSpeed = 0.00003` (UV units per second
  per m/s of wind) in `data/shaders/glsl/clouds/clouds_ps.glsl`.  Increase this
  constant for faster-moving clouds.
- Cloud colour tinting at sunset/sunrise (using `wind_time` or `world_time`) remains a
  future improvement noted in the procedural cloud history file.

## Skills / CLAUDE.md guidance followed

- `impl-issue` skill
- `src/CLAUDE.md` — Google C++ style, one class per file
- `src/editor/CLAUDE.md` — editor is the leaf of the dependency graph
