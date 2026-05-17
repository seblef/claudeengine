# Map CLI Argument Loading

**Date:** 2026-05-17
**Issue:** #250
**Branch:** feat/map-cli-arg-250

## Summary

Replaced the always-on hardcoded demo scene in `src/app/main.cpp` with an optional map loaded from a `--map <path>` CLI argument. When no map is provided the existing demo scene is kept intact.

## Changes

### `src/app/main.cpp`

- Added `#include "game/MapLoader.h"` and `#include <optional>`.
- **CLI parsing** (after `core::Config::Init()`): iterates `argv` looking for `--map <path>` or a bare positional argument (any non-flag `argv[1..n]`) as a convenience alias.
- **Lifetime design**: all scene objects that must outlive the main loop are declared at the top of `main`:
  - Map path: `map_floor`, `map_global_light`, `map_objects` (`vector<unique_ptr<GameObject>>`).
  - Demo path: `demo_floor`, `demo_obj_mesh`, `demo_fbx_mesh` (`unique_ptr<GameMesh>`); demo lights via `std::optional<game::GameLight>` (in-place construction avoids heap allocation while keeping a stable address).
- **Map load path** (when `--map` is provided):
  1. Calls `game::MapLoader::Load(map_path, video)`.
  2. On failure (`name.empty() && objects.empty()`): logs `ERROR` and falls through to the demo scene.
  3. On success:
     - Creates procedural floor with `half = map_size / 2.f` as the `CreatePlaneMesh` half-extent.
     - Creates a `GameLight(kGlobal, map_data.global_light)` for the directional light.
     - Moves `map_data.objects` into `map_objects`, adds each to the scene, and records the first `GameCamera*` found via `dynamic_cast`.
- **Camera setup** (after map loading):
  - If a `GameCamera` was found in the map: configures it (max depth, screen centre) and sets it as active — no FPS controller is registered.
  - Otherwise: configures the default `GameCamera` + `FPSCameraController` at `{0, 5, 30}` (unchanged behaviour).
- **Demo scene** is now inside `if (!map_loaded)` and mirrors the previous code exactly, using the lifetime holders declared above.

## Decisions and Rationale

- **`std::optional<game::GameLight>`** for demo lights: `GameLight` has a user-declared destructor which suppresses implicit move/copy constructors, ruling out `unique_ptr` + move and making stack allocation the only alternative. `optional::emplace()` constructs in-place without requiring move semantics, and the stored object has a stable address until the optional is destroyed.
- **`dynamic_cast`** to detect `GameCamera` in the map objects list: the `GameObjectVisitor` pattern could work, but a simple `dynamic_cast` in one place in `main.cpp` is the more readable choice and avoids a throwaway visitor class.
- **Fall-through on load failure**: consistent with the issue spec and avoids a blank screen if the path is wrong; the error is logged clearly.
- **No `SetCameraController` when map camera is found**: prevents the `FPSCameraController` from inadvertently driving the old default camera while the map camera is active.

## Skills and CLAUDE.md instructions followed

- `src/CLAUDE.md`: Google C++ style, one class per file, include root is `src/`.
- `src/game/CLAUDE.md`: MapLoader objects are NOT added to GameSystem by the loader — the caller (main) does it.
- `src/app/main.cpp` comment: "must not implement any logic other than loading configuration and running the engine" — map loading and camera wiring is app-level bootstrapping, consistent with this rule.
- Git workflow: checked out `dev`, pulled, created `feat/map-cli-arg-250`, implemented, cpplint clean, conventional commit, PR to `dev`.

## Output to keep in mind

- `MapLoader::Load` returns an empty `MapData` (name="" + objects empty) on any parse error — use this pair as the failure sentinel.
- `CreatePlaneMesh(video, half_size)` takes a **half**-extent; the current demo hardcodes `120.f` (± 120 units), while map-loaded floors use `map_size / 2.f`.
- Camera in `map_objects` must have `SetMaxDepth` and `SetScreenCenter` called by the caller — `MapLoader` does not know the screen dimensions.
