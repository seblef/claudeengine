# Editor App Entrypoint (`claude_editor`)

**Date:** 2026-05-14
**Issue:** #164
**Branch:** `feat/editor-app-entrypoint`
**PR:** #182

## What changed

Created `src/editor_app/` with two files:

### `src/editor_app/CMakeLists.txt`

Defines the `claude_editor` executable, output to `build/` (matching `claude_engine`). Links privately against `editor` only — all transitive dependencies (game, renderer, gldevices, imgui, etc.) flow through the `editor` static library.

### `src/editor_app/main.cpp`

Minimal entrypoint — no editor logic. Initialisation order matches `app/main.cpp`:

1. `core::Logger::Init` / `core::Config::Init`
2. `core::AppConfig::Init` (same `config.yaml` as the game)
3. `new core::EventManager()`
4. `gldevices::GLDevices devices(...)` — creates window + GL context
5. `new renderer::Renderer(video)` + `InitVisibilitySystems(200.f)`
6. `new editor::EditorSystem(&devices)` + `Run()`
7. Shutdown in reverse: EditorSystem → Renderer → EventManager → Logger

### `src/CMakeLists.txt`

Added `add_subdirectory(editor_app)` inside the existing `if(BUILD_EDITOR)` guard so the game build is unchanged.

## Decisions

- **No `GameSystem` in editor_app**: The game loop, camera controller, and demo objects live in `GameSystem`. The editor manages its own scene independently via `EditorSystem` (to be implemented in #165). Using `GameSystem` here would pull in game-runtime state that the editor doesn't need at the entrypoint level.
- **`RUNTIME_OUTPUT_DIRECTORY`** set to `${CMAKE_SOURCE_DIR}/build` so both binaries land in the same `build/` folder, consistent with the README.
- **`editor_app` guarded by `BUILD_EDITOR`**: since it links against `editor` which requires imgui/nfd (requiring `libgtk-3-dev`), exposing it unconditionally would break the default game-only build.

## Output to keep in mind

- `build/claude_editor` is now produced when `-DBUILD_EDITOR=ON`
- The editor startup sequence is identical to the game up through Renderer init; EditorSystem takes over from there
- Next: implement EditorSystem's ImGui lifecycle and main loop (#165)

## Skills used

- `projectSettings:impl-issue`

## CLAUDE.md notes considered

- `src/CLAUDE.md`: one class per `.cpp`/`.h`, include root is `src/`, no logic in entrypoints
- `src/editor/CLAUDE.md`: editor is the leaf; editor code must not be imported by game/renderer/etc.
- Root `CLAUDE.md`: `feat/` branch prefix, conventional commit message, cpplint before commit, history file required
