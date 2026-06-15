# EditorWindow Tool Ownership — Issue #513

## Summary

Completed the "Abstract Editor Tool System" milestone by transferring ownership of all tool instances to `EditorWindow`. This is the final wiring step after #512 (viewport tool base plumbing).

## Changes

### `EditorWindow`

- Added `std::unique_ptr<SelectionTool> selection_tool_` alongside the existing transform/placement tool members.
- Calls `viewport_->SetActiveTool(selection_tool_.get())` in the constructor (after viewport/scene wiring) so the owned selection tool is active from the first frame.
- Tool-switch block in `Render()` gained an explicit `kSelection` branch; all previous `SetActiveTool(static_cast<EditorToolBase*>(nullptr))` calls replaced with `SetActiveTool(selection_tool_.get())` — no more null-fallback.
- `sculpt_tool_` promoted from raw `EditorToolBase*` (non-owning pointer into `TerrainEditorPanel`) to `std::unique_ptr<TerrainSculptTool>` (owned).
- `WireTerrainPanel()` now constructs the `TerrainSculptTool` with foliage callbacks forwarded to `TerrainEditorPanel::OnFoliageBrush/End`, then calls `terrain_panel_.SetSculptTool(sculpt_tool_.get())` so the panel can configure the tool via its setter API.
- Terrain removal path guards `sculpt_tool_.reset()` with an active-tool switch to prevent the viewport from holding a dangling pointer.

### `TerrainEditorPanel`

- Removed `std::unique_ptr<TerrainSculptTool> sculpt_tool_` and its `SetContext()` construction block.
- `sculpt_tool_` is now a raw `TerrainSculptTool*` (not owned); set by `EditorWindow` via `SetSculptTool()`.
- Removed `GetSculptTool()` (returned ownership-from-panel approach); replaced by `SetSculptTool(tool)`, `OnFoliageBrush()`, and `OnFoliageEnd()` public methods.
- Panel Render methods (`RenderSculptTab`, `RenderPaintTab`, etc.) call setters on the raw pointer as before — no change to UI logic.

## Decisions

### Foliage callbacks as panel public methods
The foliage brush lambdas captured private panel state (`foliage_active_layer_`, `foliage_brush_mode_`, etc.). Moving them to `OnFoliageBrush()` / `OnFoliageEnd()` public methods is the cleanest boundary: EditorWindow creates the tool, panel provides the foliage logic.

### Keep viewport's internal SelectionTool
`EditorViewport` still owns a private `SelectionTool` used as a fallback in `SetActiveTool(nullptr)` and as the tool re-activated by `SetScene()`. Since EditorWindow now always passes an explicit pointer, the viewport's internal tool is only used by the `SetScene()` path (after map load, before the next Render() frame). This is invisible to the user and avoids touching the viewport.

### No sculpt_tool_active_ removal
The bool flag is still needed to gate the `SetActiveTool` calls to once-per-transition (not every frame), preventing repeated OnDeactivate/OnActivate on each frame.

## Skills/guidelines used
- `CLAUDE.md` > GUI vs. edition logic separation: panel stays pure UI, tool logic in EditorToolBase subclass.
- `CLAUDE.md` > one class per .h/.cpp pair.
- `src/CLAUDE.md` > include root is `src/`.
