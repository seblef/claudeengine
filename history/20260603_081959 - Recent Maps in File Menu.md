# Recent Maps in File Menu — Issue #376

## Summary

Added a **File > Recent** submenu to the editor that lists the 5 most recently
opened or saved maps. The list persists across sessions via `config.yaml`.

## Changes

### `src/editor/EditorWindow.h`
- Added `#include <filesystem>` (needed for the new `LoadMap` signature in the header).
- Added three private methods:
  - `LoadMap(const std::filesystem::path&)` — extracts the core load logic previously inline in `LoadFromFile`.
  - `AddToRecentMaps(const std::filesystem::path&)` — deduplicates, prepends, and trims to `kMaxRecentMaps`.
  - `SaveRecentMaps()` — round-trips `config.yaml` to persist the list.
- Added `static constexpr int kMaxRecentMaps = 5` and `std::vector<std::string> recent_maps_`.

### `src/editor/EditorWindow.cpp`
- Added `#include <fstream>` for `std::ofstream` used in `SaveRecentMaps`.
- Constructor: reads `editor.recent_maps` from `config.yaml` using `std::transform`.
- `RenderMenuBar`: inserts a `BeginMenu("Recent", !recent_maps_.empty())` submenu after "Load…" that renders each entry as a `MenuItem` (filename label + full-path tooltip, unique `##recentN` ID to avoid ImGui collisions). Clicking fires `CheckDirtyThenRun` → `LoadMap`.
- `LoadFromFile`: now just opens the NFD dialog and delegates to `LoadMap`.
- `LoadMap`: new method containing the map-loading body, calls `AddToRecentMaps` on success.
- `SaveAs`: calls `AddToRecentMaps` after a successful save.

## Decisions and rationale

- **`LoadMap` extraction**: minimal refactor required to avoid duplicating the ten-line wiring block between `LoadFromFile` and the recent-map clicks.
- **Filename label + tooltip**: keeps the submenu compact while still exposing the full path for disambiguation.
- **`CheckDirtyThenRun` for recent maps**: consistent with File > Load — the user is warned about unsaved changes before switching maps regardless of how the load was triggered.
- **Round-trip config.yaml write**: preserves unrelated config keys (graphics, shadows, etc.) by loading, patching, and re-emitting rather than overwriting the whole file.
- **`SaveAs` also registers path**: a map saved for the first time via Save As appears in Recent, matching user expectations.

## Output to remember

- `recent_maps_` is a `std::vector<std::string>` (not `path`) to simplify YAML serialisation (yaml-cpp handles `vector<string>` natively).
- The `##recentN` ImGui ID suffix prevents label-collision when two maps share the same filename.
- `cppcheck-suppress unusedStructMember` is required on `kMaxRecentMaps` (static constexpr used only in cpp, not detected by cppcheck as "used").

## Skills and instructions applied

- `impl-issue` skill (branch, implement, cpplint, commit, PR to `dev`).
- `src/editor/CLAUDE.md`: one class per file, ImGui calls within NewFrame/Render bracket, editor module is the dependency-graph leaf.
- `src/CLAUDE.md`: no unnecessary abstractions, Google C++ style, include root is `src/`.
- `CLAUDE.md` (root): conventional commit message, PR with Close #376.
