# Road Click-to-Lay Creation Flow

**Issue:** #792
**Branch:** `feat/road-click-to-lay`
**Date:** 2026-06-25

---

## Summary

Replaced the "spawn at world origin" road creation with an interactive click-to-lay flow. The user now places control points one by one by clicking the terrain, then presses **Enter** to finalise or **Escape** to cancel. The road object is not created until the first click.

---

## Changes

### `src/editor/tools/RoadTool.h`

- Added `Mode` enum (`kEditing`, `kCreating`).
- Added `OnActivateCreation()` — called by `EditorWindow` when the Create Road toolbar action fires, instead of `OnActivate()`. Sets an internal `entering_creation_mode_` flag that is consumed by the subsequent `OnActivate()` call (made by `SetActiveTool`).
- Added `IsCreating()` getter — queried by `EditorWindow` to suppress the road auto-activation logic during the creation flow.
- Added private `mode_`, `entering_creation_mode_`, and `scene_` members.
- Extracted `RenderCreating()` and `RenderEditing()` private helpers to keep `OnRender()` readable.

### `src/editor/tools/RoadTool.cpp`

- **`OnActivateCreation()`**: sets `entering_creation_mode_ = true`.
- **`OnActivate()`**: if the flag is set, enters `kCreating` mode and clears the flag; otherwise enters `kEditing` mode as before (looks up selected road).
- **`OnDeactivate()`**: resets all state including `mode_` and `scene_`; intentionally does *not* clear `entering_creation_mode_` since `OnDeactivate()` is called before `OnActivate()` inside `SetActiveTool()`.
- **`OnEvent()` (kCreating)**:
  - **Enter**: if road exists and has ≥ 3 points, sets `mode_ = kEditing`, calls `scene_->SetSelectedObject(road_)`. `EditorWindow`'s auto-logic then detects the selected road and transitions the toolbar from `kCreateRoad` to `kRoad` on the next frame, which calls `OnActivate()` and enters `kEditing` cleanly.
  - **Escape**: removes the road via `RemoveDynamicObject()`, clears `road_`, sets `mode_ = kEditing`, clears selection. The extended auto-logic in `EditorWindow` then switches the toolbar to `kSelection`.
- **`RenderCreating()`**:
  - Shows a hint overlay: *"Click to add point — Enter to finish (>= 3 pts) — Escape to cancel"*.
  - First LMB click: creates a `GameRoad` with one control point and adds it to the scene via `ctx.scene->AddDynamicObject()`.
  - Subsequent clicks: `spline.AddControlPoint(*hit)`; regenerates mesh as soon as ≥ 2 points exist.
  - Draws existing control points as grey circles.
  - Draws a yellow dashed ghost line from the last placed point to the current mouse-terrain intersection, with an open circle at the cursor position.
- **`RenderEditing()`**: unchanged editing behaviour (width slider, circles, gizmo, click-to-insert).

### `src/editor/EditorWindow.cpp`

- Replaced `CreateRoad()` call with:
  ```cpp
  road_tool_->OnActivateCreation();
  viewport_->SetActiveTool(road_tool_.get());
  ```
- Removed `CreateRoad()` method definition.
- Extended auto-activate block:
  - Guarded by `!road_tool_->IsCreating()` so it does not fire mid-creation.
  - The `!road_selected` branch now also matches `kCreateRoad` (not only `kRoad`), so Escape transitions the toolbar to `kSelection`.
- Removed now-unused `#include "game/GameRoad.h"`.

### `src/editor/EditorWindow.h`

- Removed `CreateRoad()` declaration.

---

## Key Decisions

### `entering_creation_mode_` flag instead of special `SetActiveTool` overload

`EditorViewport::SetActiveTool()` always calls `OnDeactivate()` → `OnActivate()`. Rather than adding a parallel activation path in the viewport, we use a one-shot boolean flag: `OnActivateCreation()` sets it before `SetActiveTool()` is called; `OnActivate()` consumes it. This keeps the base interface (`EditorToolBase`) unchanged.

### Road not selected during creation

The newly-created road is added to the scene but **not** passed to `SetSelectedObject()` until the user finalises with Enter. This prevents the existing auto-activate block from detecting a selected road mid-creation and prematurely switching the toolbar to `kRoad`.

### Auto-logic guard with `IsCreating()`

Rather than duplicating knowledge of what "creation mode" means in `EditorWindow`, the tool exposes `IsCreating()` and the window checks it. This keeps the two classes loosely coupled.

### Escape drives toolbar via extended auto-logic

After Escape, `mode_` is reset to `kEditing` and the selection is cleared. On the next frame the auto-logic detects `!road_selected && active_tool == kCreateRoad` and switches to `kSelection`. This avoids needing a callback or direct access to the toolbar from inside the tool.

### Ghost line

Drawn as a simple straight `AddLine` + open `AddCircle` at the cursor in yellow with 180 alpha. This matches the issue requirement of "a straight ghost line" without curved tangent preview.

---

## Scope (Out of Scope)

- Snapping to grid or existing road endpoints
- Undo/redo of individual point additions (whole road is undoable on finalise)
- Curved preview tangent visualisation

---

## Files Touched

| File | Change |
|---|---|
| `src/editor/tools/RoadTool.h` | Mode enum, OnActivateCreation(), IsCreating(), private members |
| `src/editor/tools/RoadTool.cpp` | Creation sub-state in OnEvent() and OnRender(); ghost line; hint overlay |
| `src/editor/EditorWindow.cpp` | Replace CreateRoad() call; remove CreateRoad(); extend auto-logic |
| `src/editor/EditorWindow.h` | Remove CreateRoad() declaration |

---

## Skills & Instructions Used

- `impl-issue` skill (GitHub issue #792)
- `src/CLAUDE.md`: one class per `.h`/`.cpp`, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md`: panel/tool separation, `EditorToolBase` for viewport-driven tools
- History file written per project CLAUDE.md convention
