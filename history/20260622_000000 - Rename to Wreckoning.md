# Rename project to Wreckoning

**Issue:** #705
**Branch:** chore/rename-to-wreckoning-705
**Date:** 2026-06-22

## Summary

Pure housekeeping rename of all user-facing identifiers from `ClaudeEngine` / `claude_engine` / `claude_editor` to `Wreckoning` / `wreckoning` / `wreckoning_editor`. No functional changes.

## Changes

| File | Change |
|---|---|
| `CMakeLists.txt` | `project(ClaudeEngine)` → `project(Wreckoning)`; all `[ClaudeEngine]` log tags → `[Wreckoning]`; comment referencing `claude_engine` updated |
| `src/app/CMakeLists.txt` | `add_executable(claude_engine …)` → `add_executable(wreckoning …)` |
| `src/editor_app/CMakeLists.txt` | `add_executable(claude_editor …)` → `add_executable(wreckoning_editor …)` |
| `src/app/main.cpp` | File comment, startup and shutdown log messages |
| `src/core/Logger.cpp` | Log file name `claude_engine.log` → `wreckoning.log` |
| `src/core/Logger.h` | Comment references |
| `src/gldevices/GLDevices.cpp` | GLFW window title `"ClaudeEngine"` → `"Wreckoning"` |
| `CLAUDE.md` | Engine description and CMake build target names |
| `README.md` | Title, binary names (`build/claude_engine` → `build/wreckoning`, `build/claude_editor` → `build/wreckoning_editor`), clone directory |
| `.github/workflows/ci.yml` | Artifact names, binary paths in upload/download steps, zip name, GitHub release name |

## Decisions

- **`data/config.yaml`** left unchanged — the two matching lines are absolute local filesystem paths (`/home/seb/dev/claudeengine/…`), not binary name references. Updating them would break local setups without any semantic benefit.
- **`WRECKONING.md`** left unchanged — the matching lines are the design doc's own description of the rename task being solved here. They are historical narrative, not live references.
- **`history/` files** left unchanged — they are historical records; retroactively rewriting them would falsify the project history.

## Notes for next features

- Binary outputs are now at `build/wreckoning` and `build/wreckoning_editor`.
- Log file is now `wreckoning.log` in the working directory.
- CI artifact names follow the `wreckoning-{profile}-{tag}` pattern.

## Skills used

- `impl-issue`

## CLAUDE.md / spec improvements

- The CLAUDE.md `## Building` section now correctly lists `wreckoning` and `wreckoning_editor` as the CMake targets.
- The `## Repository Overview` now describes Wreckoning as a vehicular combat game rather than a generic game engine.
