# Editor: Play/Stop Controls in EditorToolbar

**Issue**: #706 — Part of M1 (Play-in-Editor)
**Branch**: `feat/editor-play-stop-toolbar-706`

## Changes

### `EditorToolbar.h`
- Added four public setters: `SetOnPlay`, `SetOnStop`, `SetInPlayMode`, `SetPlayEnabled`.
- Added four corresponding private members: `on_play_`, `on_stop_`, `in_play_mode_`, `play_enabled_` (with `cppcheck-suppress unusedStructMember` annotations, matching the codebase convention).

### `EditorToolbar.cpp`
- **Play/Stop button** inserted after the Save button separator, before the Copy/Paste group. Uses `ICON_FA_PLAY` / `ICON_FA_STOP` labels with colour-coded styles (green in edit mode, red in play mode). Greyed out via `ImGui::BeginDisabled` when `!play_enabled_ && !in_play_mode_`.
- **Keyboard shortcuts** added inside the `!WantCaptureKeyboard` guard:
  - `F5` while not in play mode → fires `on_play_` (only when `play_enabled_`).
  - `F5` while in play mode → fires `on_stop_`.
  - `Escape` while in play mode → fires `on_stop_`.

### `EditorWindow.cpp`
- Wired `SetOnPlay` and `SetOnStop` with LOG stubs in the constructor (play mode implementation deferred to later M1 issues — `play_mode_` does not exist yet).
- Added `toolbar_->SetPlayEnabled(...)` call at the top of `Render()`, enabled only when a map name is set **and** the scene has a file path (i.e., the map has been saved at least once).

## Decisions

- **Stub wiring**: `play_mode_->Enter()` / `play_mode_->Exit()` cannot be wired yet because no `EditorPlayMode` class exists. Stubs log the intent and will be replaced in the first M1 implementation issue.
- **Button placement**: between Save and Copy/Paste, matching the spec. The visual separation (`SeparatorEx`) is consistent with the rest of the toolbar.
- **`play_enabled_` condition**: requires both a non-empty map name and a saved file path. This matches the issue spec ("no map is loaded or map has no file path") and prevents launching a play session on an unsaved/untitled map.
- **Colour approach**: Three-slot `PushStyleColor` (Button, ButtonHovered, ButtonActive) gives a proper hover/press feedback rather than just tinting the normal state.

## Notes for next features
- When `EditorPlayMode` is introduced, replace the LOG stubs in `EditorWindow.cpp` constructor with `play_mode_->Enter()` and `play_mode_->Exit()`.
- `SetInPlayMode(true/false)` must be called by whoever owns the play mode state so the toolbar button label/colour stays in sync.
- F5 shortcut fires even when the toolbar window is not focused (it runs in the global `!WantCaptureKeyboard` block), which is the intended UX.

## Skills / instructions used
- `impl-issue` skill
- `src/CLAUDE.md` — one class per file, Google C++ style guide, include root is `src/`
- `src/editor/CLAUDE.md` — panel classes must be pure UI; build with `BUILD_EDITOR=ON`
- Root `CLAUDE.md` — cpplint before commit, conventional commits, history file required
