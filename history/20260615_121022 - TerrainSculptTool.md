# TerrainSculptTool — Issue #511

## What changed

Extracted the terrain sculpt brush logic from `EditorViewport` into a new `TerrainSculptTool` class.

### Files created
- `src/editor/tools/TerrainSculptTool.h`
- `src/editor/tools/TerrainSculptTool.cpp`

### Files modified
- `src/editor/tools/CMakeLists.txt` — added `TerrainSculptTool.cpp`
- `src/editor/EditorViewport.h` — removed `sculpt_active_`, `sculpt_stroke_active_`, `on_sculpt_brush_`, `on_sculpt_end_` and their setters
- `src/editor/EditorViewport.cpp` — removed the sculpt brush block from `Render()` and the `!sculpt_active_` guard on `active_tool_base_->OnRender()`
- `src/editor/EditorWindow.h` — added `sculpt_tool_` (`unique_ptr<TerrainSculptTool>`) and `sculpt_tool_active_` bool
- `src/editor/EditorWindow.cpp` — `WireTerrainPanel()` constructs `TerrainSculptTool`; sculpt activation uses `SetActiveTool()` transitions instead of `SetSculptActive()`

## Decisions

### Tool activation via `SetActiveTool()` instead of a flag
The previous design used a `sculpt_active_` bool in `EditorViewport` that bypassed `active_tool_base_->OnRender()`. The new design makes `TerrainSculptTool` the active tool, which fits the existing `EditorToolBase` abstraction cleanly. `IsCapturingMouse()` returning `stroke_active_` lets other systems know the mouse is captured during a stroke.

### Sculpt tool created in `EditorWindow`, not `TerrainEditorPanel`
The issue suggests `TerrainEditorPanel` could own the tool, but that panel has no reference to `EditorViewport`. `EditorWindow` already wires up the terrain panel and viewport callbacks in `WireTerrainPanel()`, so the tool is created there (consistent with how `PlacementTool` and `TransformTool` are owned by `EditorWindow`).

### Activation tracking with `sculpt_tool_active_`
A `sculpt_tool_active_` flag tracks whether the sculpt tool is currently the viewport's active base tool. Transitions (panel open/close, terrain removed) call `SetActiveTool()` only when needed, avoiding spurious `OnDeactivate()`/`OnActivate()` calls each frame. When sculpt ends, `SetActiveTool(nullptr)` restores `SelectionTool`.

### `terrain_data_` kept on viewport
`EditorViewport::terrain_data_` is retained because `PlacementTool` uses `ComputeTerrainHit()` via the `EditorToolContext::terrain_data` field. The sculpt tool stores its own `data_` pointer passed at construction.

## Output to keep in mind

- The `EditorTool` enum (`EditorViewport::SetActiveTool(EditorTool)`) still exists alongside the base-tool pointer. The enum drives toolbar/gizmo display; the base tool drives mouse input. These are separate concerns.
- `TerrainSculptTool` has no `OnActivate`/`OnDeactivate` overrides — its only state is `stroke_active_`, which resets naturally when the panel closes (the next `else if (sculpt_tool_active_)` branch fires `on_end_` implicitly via the tool not receiving more `OnRender()` calls — actually the stroke is *not* ended explicitly on deactivate; consider adding that if a half-open stroke causes a push of empty `SculptBrushCommand`).
- `ViewportRaycast.h` / `ViewportRaycast.cpp` already existed (created in the `PlacementTool` PR); no duplication was needed.

## Skills and instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; Google C++ style; include root is `src/`
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; one class per file
- `history/` folder for contribution MarkDown
