# Hover BBox Highlight in Selection and Transform Tools

**Issue**: #696
**Branch**: feat/ui-editor-hover-bbox-696
**Date**: 2026-06-20

## Summary

Added a semi-transparent white wireframe bounding box drawn around the object under the cursor in the SelectionTool and TransformTool. The highlight gives the user a visual cue about which object will be selected before clicking, using the exact same triangle-precision ray cast as the actual selection.

## Changes

### `PickingUtils.h` / `PickingUtils.cpp`

- **Added `PickHitAt()`**: Extracted the full multi-pass hit-test body from `PickObjectAt()` into a new free function that returns the nearest `game::GameObject*` without modifying the scene selection. `PickObjectAt()` now delegates to `PickHitAt()` and applies the result (toggle or replace).
- **Added `DrawHoverBBox()`**: Draws a single object's bbox in `IM_COL32(255, 255, 255, 160)` at 1.0 px thickness, reusing the existing `DrawBBoxWireframe()` helper.

### `SelectionTool.h` / `SelectionTool.cpp`

- Added `hovered_object_` (`game::GameObject*`) and `last_hover_mouse_pos_` (`ImVec2`) private members.
- In `OnRender()`, before drawing the orange selection bbox: evaluate hover guards (`ImGui::IsWindowHovered()`, `!ImGuizmo::IsOver()`, `!ImGuizmo::IsUsing()`), re-cast via `PickHitAt()` only when the mouse position has changed, then call `DrawHoverBBox()` if the result is not already selected.
- `OnDeactivate()` resets both members.

### `TransformTool.h` / `TransformTool.cpp`

- Same hover state members as SelectionTool.
- Added `OnDeactivate()` override (was missing) to reset hover state.
- Hover logic placed **before** the early `return` that fires on empty selection, so hover feedback is visible even when no object is currently selected.
- Added `#include "editor/tools/PickingUtils.h"` to the header.

## Decisions

- **Extracting `PickHitAt()`** instead of duplicating the ray-cast inline: keeps `PickObjectAt()` as the single source of truth for picking logic, and makes `PickHitAt()` independently reusable for any future read-only query (e.g. tooltip display, cursor icon changes).
- **Mouse-move guard** (`mouse_pos != last_hover_mouse_pos_`): the ray cast (including triangle intersection) is skipped entirely when the mouse hasn't moved. This makes the per-frame cost of hover negligible (just a comparison).
- **Placement in TransformTool**: hover logic runs unconditionally before the `if (selection.empty()) { ... return; }` block, so it works correctly with and without a selection.
- **Suppression when `ImGuizmo::IsOver() || ImGuizmo::IsUsing()`**: clears `hovered_object_` so the white bbox disappears while the gizmo handle is hovered or being dragged — avoids distracting highlights during transform operations.

## For Future Reference

- `DrawBBoxWireframe()` is a file-scope anonymous function in `PickingUtils.cpp`; it handles perspective-correct projection and clips edges where either endpoint is behind the camera. Reuse it for any future bbox-style overlays.
- The hover pattern (cache + mouse-move guard) can be replicated in any future tool that needs cheap per-frame hit-testing.
- Floating ImGui panels on top of the viewport are handled automatically: `ImGui::IsWindowHovered()` uses ImGui's z-order hit-testing and returns false when the cursor is over a higher-priority window.

## Skills / CLAUDE.md Instructions Applied

- `src/CLAUDE.md`: one class per `.h`/`.cpp`, Google C++ style guide, include root is `src/`.
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; tools implement editing logic, panels stay pure UI.
- Conventional commits for the commit message.
- History file required for every contribution.
