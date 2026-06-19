# SoundEmitter Wireframe Gizmos — Issue #671

## What was done

Implemented viewport wireframe gizmos for `kSoundEmitter` objects (issue #671).

### Files changed

| File | Change |
|---|---|
| `src/game/GameSoundEmitter.h/.cpp` | Added `GetMinDistance()` / `GetMaxDistance()` getters |
| `src/editor/SoundEmitterGizmos.h` | New header declaring `EnqueueSoundEmitterGizmos()` |
| `src/editor/SoundEmitterGizmos.cpp` | New implementation of the gizmo enqueue function |
| `src/editor/EditorViewport.cpp` | Calls `EnqueueSoundEmitterGizmos` after `EnqueuePlayerStartGizmos` |
| `src/editor/CMakeLists.txt` | Added `SoundEmitterGizmos.cpp` to the editor library |

## Key decisions

### Overlay vs depth-tested rendering

Used `PushOverlaySphere` (always-on-top, no depth test) rather than `PushSphere`
(depth-tested like lights). Rationale: sound emitters have no visible geometry, so
depth-testing would cause the spheres to disappear behind terrain/meshes, making
them impossible to locate. Overlay rendering mirrors the `PlayerStartGizmos` approach.

### Color choice

Teal `(0, 0.75, 0.75)` / bright teal `(0.2, 1.0, 1.0)` on selection. Distinct from:
- Light gizmos (the light's own color, or blue `(0, 0.53, 1)` highlight)
- PlayerStart gizmos (green flag, white pole)
- Physics debug (yellow/green Jolt defaults)

### Zoom-adaptive minimum radius

A minimum radius = `camera_distance * 0.04` is enforced:
- Inner sphere: `max(min_distance, min_vis)`
- Outer sphere: `max(max_distance, max(inner * 1.5, 2 * min_vis))`

This ensures both spheres remain visually distinct at any zoom level, even when
the actual audio distances are small (e.g. 0.5 m / 3 m with camera 100 m away).

### Getter placement

`GetMinDistance()` / `GetMaxDistance()` were added to `GameSoundEmitter` rather than
making the gizmo code depend on `audio::Sound` directly. This keeps the editor layer
clean (it only depends on `game`), matches the existing pattern of `GetSoundName()`.

## Output to keep in mind

- `EnqueueSoundEmitterGizmos` takes a `camera_distance` float (from
  `camera_ctrl_->GetDistance()`); if additional gizmo types need zoom-adaptive
  sizing in the future, consider a shared helper or passing the full camera.
- The `kMinGizmoScale = 0.04f` constant is empirically chosen; if users find
  gizmos too large at close range it can be reduced to ~0.02.
- The outer-sphere minimum of `r_inner * 1.5f` may overcorrect when
  `min_distance` is large and `max_distance` is only slightly larger — future
  tuning might cap the minimum gap differently.

## Skills and CLAUDE.md instructions used

- `impl-issue` skill
- `src/CLAUDE.md` — one class per file, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md` — editor is the leaf of the dependency graph; GUI/logic separation
- `src/game/CLAUDE.md` — `GameObject` subclasses are scene objects; `GameSoundEmitter` pattern
- `src/audio/CLAUDE.md` — `SoundDesc` fields (`min_distance`, `max_distance`)
