# EditorSystem ImGui Lifecycle and Main Editor Loop

**Date:** 2026-05-14
**Issue:** #165
**Branch:** `feat/editor-system-imgui-loop`
**PR:** #183

## What changed

### `src/editor/EditorSystem.h`

Added:
- `~EditorSystem()` destructor (ImGui shutdown)
- `abstract::VideoDevice* video_` member (for `BeginFrame` / `ClearRenderTargets`)
- `std::unique_ptr<EditorWindow> editor_window_` member
- Forward declaration of `EditorWindow`
- `cppcheck-suppress unusedStructMember` on all private pointer/unique_ptr members (cppcheck only sees the header, not the .cpp usage)

### `src/editor/EditorSystem.cpp`

Full implementation:
1. Constructor: `IMGUI_CHECKVERSION` → `CreateContext` → docking flag → `ImGui_ImplGlfw_InitForOpenGL` (casts `devices_->GetWindow()` to `GLFWwindow*`) → `ImGui_ImplOpenGL3_Init("#version 460")` → `StyleColorsDark` → `make_unique<EditorWindow>()`
2. `Run()` loop: pump events → early-exit on `kWindowClose` → `BeginFrame`/`ClearRenderTargets` → ImGui new-frame trio → `editor_window_->Render()` → `ImGui::Render()` → `RenderDrawData`
3. Destructor: `ImGui_ImplOpenGL3_Shutdown` → `ImGui_ImplGlfw_Shutdown` → `DestroyContext`

### `src/editor/EditorWindow.h` / `.cpp`

New stub class. `Render()` is a no-op. Will be implemented in issue #166.

### `src/editor/CMakeLists.txt`

Added `EditorWindow.cpp` to the `editor` STATIC library sources.

## Decisions

- **Early break after `running_ = false`**: After draining events, if `running_` was just cleared by a `kWindowClose`, the loop breaks immediately rather than starting a new frame. Avoids rendering to a closed window and making redundant GPU calls.
- **`GetWindow()` cast**: `abstract::Devices::GetWindow()` returns `void*`; `imgui_impl_glfw.h` forward-declares `struct GLFWwindow`, so the cast in the `.cpp` requires no extra GLFW include — the pointer type is complete for ImGui's use.
- **`cppcheck-suppress` placement**: cppcheck's `unusedStructMember` checks headers in isolation without resolving cross-TU usage; the suppress annotations follow the same pattern established in the original skeleton.
- **`EditorWindow` stub**: keeps this PR focused on the lifecycle. Issue #166 will add the docking layout, menu bar, toolbar, and status bar.

## Output to keep in mind

- `EditorWindow::Render()` is the single per-frame entry point for all ImGui panels; everything (viewport, resource tree, object tree, log panel) will be called from there
- `video_->BeginFrame()` resets the viewport to full-screen dimensions — important to know for the viewport rendering-to-texture approach in #168/#169
- Next: `EditorWindow` full docking layout (#166)

## Skills used

- `projectSettings:impl-issue`

## CLAUDE.md notes considered

- `src/editor/CLAUDE.md`: all ImGui calls bracketed by `NewFrame()`/`Render()`; no singleton subsystems inside EditorSystem (EditorWindow is owned via `unique_ptr`)
- `src/CLAUDE.md`: one class per `.cpp`/`.h`
- Root `CLAUDE.md`: `feat/` prefix, conventional commit, cpplint + cppcheck before commit, history file required
