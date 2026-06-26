# PlacementTool Position Snapping

**Date:** 2026-06-26  
**Branch:** `feat/placement-tool-snapping`  
**PR:** #816  
**Closes:** #811

## Changes

### `src/editor/EditorUtils.h`

Added an inline helper used by `PlacementTool`:

```cpp
inline float SnapValue(float value, float snap) {
  return std::round(value / snap) * snap;
}
```

This lives in the shared editor utilities header alongside the existing angle-conversion helpers. No new file was needed.

### `src/editor/tools/PlacementTool.h` / `.cpp`

- Forward-declared `EditorToolbar` in the header.
- Added `SetToolbar(const EditorToolbar*)` setter and a `const EditorToolbar* toolbar_ = nullptr` member, mirroring the `TransformTool` pattern exactly.
- In `UpdatePreviewPosition()`: after computing the terrain hit, extract `x` and `z`, round each through `SnapValue` when `toolbar_->IsSnapEnabled()`, and pass the snapped values to `SetWorldTransform`. Y (`hit->y + preview_height_`) is intentionally left untouched so the object rests on the terrain surface.

### `src/editor/EditorWindow.cpp`

Called `placement_tool_->SetToolbar(toolbar_.get())` immediately after each of the seven `std::make_unique<PlacementTool>(...)` calls, before `viewport_->SetActiveTool(placement_tool_.get())`. The sites cover: lights, pivots, player starts, meshes (from modal), particle systems, sound emitters, and vehicles.

## Decisions

- **Setter over constructor injection** — the issue mentioned passing a toolbar at construction, but the project already uses `SetToolbar()` for `TransformTool`. Consistency was preferred over changing the constructor signature.
- **`SnapValue` in `EditorUtils.h`** — the issue allowed placing it locally in `PlacementTool.cpp`. Putting it in the shared header makes it available if future tools need it, without introducing a new file or any circular dependency.
- **Y axis excluded** — deliberately not snapped; the object must follow the terrain surface regardless of grid state.

## Key invariants to keep in mind

- `toolbar_` is set after construction and before `OnActivate`; it is guaranteed to outlive the tool because `EditorWindow` owns both.
- If `toolbar_` is `nullptr` (e.g., a unit test that constructs `PlacementTool` without wiring a toolbar), the guard `if (toolbar_ && ...)` prevents a crash.

## Skills / CLAUDE.md instructions followed

- `src/editor/CLAUDE.md` — one class per file pair; `EditorWindow` GUI/edition separation.
- `src/CLAUDE.md` — Google C++ style; include root is `src/`; utility functions in shared `*Utils` files.
- Root `CLAUDE.md` — conventional commit message; cpplint clean; history file; PR with issue-close command.
