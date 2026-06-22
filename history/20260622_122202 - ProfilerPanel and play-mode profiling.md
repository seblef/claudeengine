# ProfilerPanel and play-mode CPU profiling (Issue #723)

## What was done

- Activated `core::Profiler` during play mode via `PlayModeManager`.
- Created `editor::ProfilerPanel`: a read-only ImGui panel showing FPS and per-scope timing data.
- Wired the panel to auto-appear when play starts and hide when play stops; also togglable via **View > Profiler** (disabled outside play mode).
- Added `PROFILE_SCOPE` instrumentation to four hot paths.
- Added `GetReportInterval()` accessor to `core::Profiler`.

## Files changed

| File | Change |
|---|---|
| `src/core/Profiler.h` | Added `GetReportInterval()` const accessor |
| `src/editor/ProfilerPanel.h` | New class — read-only profiling UI panel |
| `src/editor/ProfilerPanel.cpp` | Renders FPS header + colour-coded table of scopes |
| `src/editor/PlayModeManager.h` | Added `kProfilerInterval = 2.0` constant |
| `src/editor/PlayModeManager.cpp` | Enter() → `new Profiler`, Tick() → `MarkFrame()` + two PROFILE_SCOPEs, Exit() → `Profiler::Shutdown()` |
| `src/editor/EditorWindow.h` | Added `profiler_panel_` (unique_ptr) and `show_profiler_panel_` flag |
| `src/editor/EditorWindow.cpp` | Include, construct, wire callbacks, render panel, View > Profiler menu item |
| `src/editor/CMakeLists.txt` | Added `ProfilerPanel.cpp` |
| `src/renderer/Renderer.cpp` | `PROFILE_SCOPE("Renderer::Draw")` at the top of `Update()` |
| `src/game/FPSCameraController.cpp` | `PROFILE_SCOPE("FPSCamera::Update")` at the top of `Update()` |

## Key decisions

**`IsInstanced()` not `HasInstance()`**: The `Singleton` template exposes `IsInstanced()`. The issue mentioned `HasInstance()` which does not exist — using the correct API.

**`GetReportInterval()` added to Profiler**: The panel spec required showing the interval in the FPS header line. Since `report_interval_s_` was private with no getter, I added a trivial inline accessor rather than plumbing the interval through `PlayModeManager` → `EditorWindow` → `ProfilerPanel`.

**`cppcheck-suppress shadowVariable`**: The `PROFILE_SCOPE` macro expands to a named local variable using `##__LINE__`. Cppcheck cannot expand `__LINE__` at check time and incorrectly flags nested `PROFILE_SCOPE` calls as shadowing. The suppression is applied on the inner scope only.

**Panel visibility**: The profiler panel is rendered outside the `ArePanelsHidden()` block — it is the only panel that is exclusively visible *during* play mode (when all other panels are hidden). If `show_profiler_panel_` is true and the user clicks close (the `&` flag), ImGui sets it false; no additional wiring needed.

**Profiler placement in `Tick()` ordering**: `MarkFrame()` is called before physics and camera updates, matching the pattern of counting the frame that includes those operations. The outer `PROFILE_SCOPE("PlayModeManager::Tick")` wraps the whole tick; the inner `PROFILE_SCOPE("Physics::Step")` wraps only the step call.

## Output to keep in mind

- `core::Profiler` is a no-op Singleton pattern: zero overhead when not instantiated, so `PROFILE_SCOPE` macros placed in renderer/game code are safe at all times.
- GPU timing is explicitly out of scope (noted in issue); future work should use timer queries via `abstract/`.
- Profiler settings (interval) are not persisted to `config.yaml`; the 2-second default is a compile-time constant.

## Skills and CLAUDE.md rules used

- `src/editor/CLAUDE.md` — GUI vs edition logic separation (ProfilerPanel is pure UI, no mutations).
- `src/editor/CLAUDE.md` — `cppcheck-suppress unusedStructMember` for class members; full `#include` for value members in EditorWindow.h.
- `src/CLAUDE.md` — one class per `.h`/`.cpp` pair; include root is `src/`.
