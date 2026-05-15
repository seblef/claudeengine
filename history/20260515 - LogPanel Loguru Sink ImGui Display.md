# Log Panel: Loguru Sink with ImGui Scrolling Display (Issue #178)

## Changes

### `src/editor/LogPanel.h`
- Full class declaration replacing the empty stub.
- `#include <deque>`, `<mutex>`, `<string>`, `<loguru.hpp>`.
- `LogEntry` inner struct (`verbosity` + `text`), both suppressed from cppcheck `unusedStructMember`.
- `entries_` (`std::deque<LogEntry>`), `mutex_` (`std::mutex`), `scroll_to_bottom_` (`bool`).
- `static void LogCallback(void*, const loguru::Message&)` — loguru sink.
- `void Render()` — ImGui display.

### `src/editor/LogPanel.cpp`
- `kMaxEntries = 500` constant in anonymous namespace.
- `ColourForVerbosity(Verbosity)` helper: WARNING → yellow `{1,1,0,1}`, ≤ ERROR → red `{1,0.3,0.3,1}`, else white.
- `LogCallback`: acquires mutex, pops front if at cap, pushes `{msg.verbosity, msg.message}`, sets `scroll_to_bottom_`.
- `Render`: acquires mutex, `BeginChild("##logs", {0,0}, ImGuiChildFlags_None, HorizontalScrollbar)`, iterates entries with `TextColored`, calls `SetScrollHereY(1.0f)` then clears flag, `EndChild`.

### `src/editor/EditorWindow.cpp`
- Constructor: `loguru::add_callback("editor_log", &LogPanel::LogCallback, log_panel_.get(), loguru::Verbosity_INFO)`.
- Destructor: replaced `= default` with body calling `loguru::remove_callback("editor_log")`.

## Decisions

- **Callback in EditorWindow not EditorSystem**: The issue spec suggested EditorSystem but `log_panel_` is owned by `EditorWindow`. Registering in the owner is cleaner; no accessor API needed.
- **Mutex in Render()**: loguru may fire the callback from any thread. Holding the mutex during the full render loop is safe since `Render()` is called from the main thread and the lock duration is short (just deque iteration).
- **`ImGuiChildFlags_None` not `false`**: The bundled ImGui (docking) uses the newer `BeginChild(id, size, ImGuiChildFlags, ImGuiWindowFlags)` signature. Using `false` (= 0) would compile but is misleading; `ImGuiChildFlags_None` is explicit.
- **`msg.message` not `msg.preamble + msg.message`**: The issue spec uses `msg.message` (user text only). The loguru `preamble` (timestamp/file/line) is already sent to stderr; the panel shows just the message for readability.

## Output to keep in mind

- The `mutex_` is held during `Render()`. If the panel ever becomes slow (many entries), consider double-buffering (swap a local copy under the lock, render without it).
- `cppcheck` flags `unusedStructMember` on inner struct fields even when they are used by name in `.cpp`. The `// cppcheck-suppress` workaround is the same pattern used throughout the codebase.

## Skills used
- `projectSettings:impl-issue`

## CLAUDE.md notes taken into account
- One class per `.h`/`.cpp` pair.
- Editor is the dependency-graph leaf.
- Google C++ style guide; cpplint + cppcheck pre-commit hooks pass.
