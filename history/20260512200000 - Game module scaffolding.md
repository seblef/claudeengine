# Game module scaffolding

**Date:** 2026-05-12
**Issue:** #124
**PR:** #133
**Branch:** feat/game-module-scaffolding

## Changes

- `src/game/CMakeLists.txt` — static library `game`, links `core`, `abstract`, `renderer`
- `src/CMakeLists.txt` — added `add_subdirectory(game)` between `renderer` and `app`
- `src/app/CMakeLists.txt` — added `game` to `claude_engine` link libraries
- `src/game/game.cpp` — placeholder TU (CMake requires at least one source for STATIC)
- `src/game/CLAUDE.md` — module documentation

## Decisions

**Placeholder `game.cpp`:** CMake's `add_library(STATIC)` requires at least one source file. A minimal placeholder (`namespace game {}`) was used rather than switching to `INTERFACE` or `OBJECT` library type, since the target will receive real sources immediately in subsequent issues and `STATIC` is correct for the final shape.

**Link order:** `game` is added to `src/CMakeLists.txt` after `renderer` and before `app`, making the dependency flow explicit: `app → game → renderer → abstract → core`.

## Output to keep in mind

- `game.cpp` placeholder should be removed once the first real `.cpp` file (e.g. `GameObject.cpp`) is added in issue #125.
- All subsequent game module issues must add their `.cpp` files to `src/game/CMakeLists.txt` under `add_library(game STATIC ...)`.

## Skills / instructions used

- `src/CLAUDE.md`: new modules need a `CMakeLists.txt` and an entry in `src/CMakeLists.txt`
- Root `CLAUDE.md`: git workflow (feat/ branch → cpplint → conventional commit → PR to dev)
