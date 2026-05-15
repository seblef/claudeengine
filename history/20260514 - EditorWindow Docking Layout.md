# EditorWindow Docking Layout and Panel Stubs

**Date:** 2026-05-14
**Issue:** #166
**Branch:** `feat/editor-window-docking-layout`
**PR:** #184

## What changed

### `src/editor/EditorWindow.h` / `.cpp`

EditorWindow rewritten from the placeholder stub. `Render()` now builds the full ImGui layout:
1. `ImGui::DockSpaceOverViewport()` — root dockspace (no args; defaults to main viewport with the new `dockspace_id=0, viewport=NULL` API)
2. `BeginMainMenuBar` / `EndMainMenuBar` — empty placeholder
3. `toolbar_->Render()` — stub, wired in #174
4. `ImGui::Begin("Viewport")` + `viewport_->Render()` — wired in #169
5. `ImGui::Begin("Scene")` + `BeginTabBar("##scene_tabs")` with "Resources" (#175) and "Objects" (#176) tabs
6. `ImGui::Begin("Properties")` — empty, populated in next milestone
7. `ImGui::Begin("Logs")` + `log_panel_->Render()` — wired in #178
8. Status bar: `SetNextWindowPos`/`SetNextWindowSize` to pin to screen bottom, `NoTitleBar|NoResize|NoMove|NoScrollbar` flags

Uses forward declarations for all panel types + explicit destructor declaration (defined `= default` in .cpp) so `unique_ptr<T>` of incomplete types compiles correctly.

### New stub files

Each has a default constructor and empty `Render()` method:
- `EditorToolbar.h/.cpp`
- `EditorViewport.h/.cpp`
- `ResourcesPanel.h/.cpp`
- `ObjectsPanel.h/.cpp`
- `LogPanel.h/.cpp`

### `editor/CMakeLists.txt`

Added all 5 new `.cpp` files.

## Decisions

- **`DockSpaceOverViewport()` vs `DockSpaceOverViewport(ImGui::GetMainViewport())`**: The issue spec used the old API signature. In the fetched ImGui docking version, the first parameter is `ImGuiID dockspace_id` (not `const ImGuiViewport*`). Used `DockSpaceOverViewport()` with no args (defaults to main viewport) which is the documented idiomatic usage.

- **`ImGui::TextUnformatted("")` instead of `ImGui::Text("")`**: `ImGui::Text` has `__attribute__((format, printf, 1, 2))` — an empty format string triggers `-Wformat-zero-length`. `TextUnformatted` has no format attribute, so it's the correct API for outputting a literal string without format interpretation.

- **Forward declarations + explicit destructor**: `EditorWindow.h` only forward-declares the 5 panel types so that `unique_ptr<Incomplete>` is valid as a member. The destructor is declared in `.h` and defined `= default` in `.cpp` (after the full type includes), which is the standard pattern for pimpl-style unique_ptrs.

- **Panel stubs**: Kept fully minimal (no members, empty Render). Implementation context is deferred to the individual issues (#169, #174–#178).

## Output to keep in mind

- `EditorWindow::Render()` is the single call site for all panel rendering — all future panels hang off this method
- Status bar is positioned manually; the dockspace will by default cover the full viewport including the status bar area. Use `imgui.ini` to configure panels to leave the status bar visible, or resize the dockspace in a future milestone
- The "Scene" tab bar ID is `"##scene_tabs"` (double `##` hides it from display)
- Next: EditorCameraController (#167) and render-to-texture (#168/#169)

## Skills used

- `projectSettings:impl-issue`

## CLAUDE.md notes considered

- `src/editor/CLAUDE.md`: one class per `.h`/`.cpp`, all ImGui calls bracketed by `NewFrame()`/`Render()`, no singleton subsystems
- `src/CLAUDE.md`: one class per `.cpp`/`.h`
- Root `CLAUDE.md`: `feat/` prefix, conventional commit, cpplint + cppcheck before commit, history file required
