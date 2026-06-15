# TerrainSculptTool — Brush Algorithm Consolidation

## What changed

Moved all brush algorithms (sculpt, paint, foliage dispatch) from
`TerrainEditorPanel` into `TerrainSculptTool`, making the panel pure UI.

### Files modified

- `src/editor/tools/TerrainSculptTool.h` — expanded from a thin callback-wrapper
  to a self-contained brush engine: `Mode {kSculpt, kPaint, kFoliage}`,
  `Tool {kRaise, kLower, kSmooth, kFlatten}`, `Falloff {kLinear, kSmooth}` enums;
  full setter/getter API for brush parameters; all stroke state members;
  foliage callbacks stored as `std::function<>` members.

- `src/editor/tools/TerrainSculptTool.cpp` — all brush algorithms now live here:
  `ComputeFalloff`, `EnsureRegionCovers`, `ApplyBrushFootprint`, `OnSculptAt`,
  `OnSculptEnd`, `EnsurePaintRegionCovers`, `ApplyPaintFootprint`, `OnPaintAt`,
  `OnPaintEnd`. `OnRender()` dispatches based on `mode_`.

- `src/editor/TerrainEditorPanel.h` — removed `Tool`, `Falloff` enums; removed
  brush helper declarations; removed brush parameter and stroke state members;
  added `unique_ptr<TerrainSculptTool> sculpt_tool_`; added `GetSculptTool()`.

- `src/editor/TerrainEditorPanel.cpp` — removed `OnBrushAt`, `OnBrushEnd` and all
  brush implementations. `SetContext()` now creates `TerrainSculptTool` with foliage
  lambdas. `Render()` calls `sculpt_tool_->SetMode()` on each active tab.
  `RenderSculptTab()` and `RenderPaintTab()` read/write params via getters/setters.

- `src/editor/EditorWindow.h` — removed `#include "editor/tools/TerrainSculptTool.h"`;
  changed `unique_ptr<TerrainSculptTool> sculpt_tool_` to `EditorToolBase* sculpt_tool_ = nullptr`.

- `src/editor/EditorWindow.cpp` — `WireTerrainPanel()` simplified: no longer
  constructs `TerrainSculptTool` directly; calls `terrain_panel_.GetSculptTool()`.

## Decisions

### TerrainEditorPanel owns the tool
The tool is created in `SetContext()` alongside all other terrain context. This
is the natural location: the panel is the one that knows the foliage tab's state
(active layer, brush mode, radius, strength) which the foliage callbacks close
over. `EditorWindow` only holds a non-owning raw pointer so it can activate the
tool in the viewport.

### Foliage mode via callbacks, not context injection
Foliage brushing needs `GameTerrain*` and `FoliageLayer`, which live in the
panel's context. Rather than giving the tool those pointers (broadening its
interface and coupling it to `game::GameTerrain`), we pass lambdas that capture
`this` on construction. The tool fires them; the panel implements them.

### Mode sync via `SetMode()` in `Render()`
ImGui's `BeginTabItem` returns `true` only for the active tab. Each active tab
calls `sculpt_tool_->SetMode(...)`. This is idiomatic: no separate mode-tracking
member on the panel, and the mode stays correct even if the user switches tabs
while dragging (the stroke ends naturally when LMB is released).

### SliderFloat pattern for tool parameters
Since the brush parameters now live in the tool (not in the panel), sliders use a
local-copy pattern: read → slider → conditional setter. This avoids a pointer into
the tool's internals and lets the setter run any side-effects cleanly.

## Output to keep in mind

- `TerrainPainterWindow::ApplyPainting()` was intentionally left in place — it is
  a full-terrain button-triggered operation, not a brush stroke, and does not belong
  in `TerrainSculptTool`.
- `foliage_stroke_active_` was removed from `TerrainEditorPanel`; the tool tracks
  it internally as `foliage_stroke_active_`.
- The old `OnBrushAt` / `OnBrushEnd` public API is fully gone from the panel;
  `EditorWindow` no longer calls those methods.

## Skills and instructions used

- `impl-issue` skill (initial TerrainSculptTool extraction, same branch)
- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; Google C++ style; include root is `src/`
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; one class per file
- `history/` folder for contribution Markdown
