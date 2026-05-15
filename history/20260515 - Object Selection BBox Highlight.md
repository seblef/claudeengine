# Object Selection by Click and Bounding Box Wireframe Highlight

**Issue:** #172  
**Branch:** `feat/object-selection-bbox-highlight`  
**Date:** 2026-05-15

## Summary

Implements scene object picking by mouse click (ray–AABB intersection) and highlights the selected object with an orange wireframe bounding box drawn via ImGui.

## Changes

### `src/core/BBox3.h`
Added a second overload of `IntersectsRay` that accepts a `float& t_out` output parameter. It returns the t value at the nearest hit point (0 if the ray origin is inside the box), enabling nearest-hit selection among multiple candidates. The existing no-output overload is unchanged.

### `src/editor/EditorToolbar.h`
Added `IsSelectionToolActive() const → bool` stub, always returning `true`. This is the hook for issue #174 (full toolbar implementation) to enable/disable object picking without touching the viewport code.

### `src/editor/EditorViewport.h`
- Added `SetSelectionActive(bool)` public method to let `EditorWindow` wire the toolbar selection state.
- Added `selection_active_` private field (default `true`).
- Declared private helpers `PickObjectAt()` and `DrawSelectedBBox()`.

### `src/editor/EditorViewport.cpp`
**Picking (`PickObjectAt`):**
1. Converts mouse screen position to NDC: `ndc_x = (mx - img_x) / w * 2 - 1`, Y flipped.
2. Unprojects via `inv(ViewProjection) * (ndc_x, ndc_y, -1, 1)^T` to a world-space point on the near plane.
3. Computes world-space ray direction as `normalize(world_point - camera_position)`.
4. Tests each scene object's `GetWorldBBox()` with `IntersectsRay(..., t)`, skipping lights (infinite bbox).
5. Selects the nearest hit; deselects if no hit.

**Picking trigger in `Render()`:**
- Fires on `ImGui::IsMouseReleased(Left)` when the viewport is hovered, the XYZ overlay is not hovered, and `KeyAlt` is not held (Alt+LMB is camera orbit).
- `image_pos` is captured via `ImGui::GetCursorScreenPos()` *before* `ImGui::Image()` to get the exact rendered image origin regardless of title bar height.

**Wireframe (`DrawSelectedBBox`):**
- Projects all 8 AABB corners through the camera view-projection matrix.
- Skips any edge where either endpoint has `clip.w ≤ 0` (behind the camera near plane).
- Draws 12 edges (4 front face, 4 back face, 4 pillars) in orange `IM_COL32(255, 165, 0, 255)` at 1.5px width using `ImDrawList::AddLine`.

### `src/editor/EditorWindow.cpp`
Added `viewport_->SetSelectionActive(toolbar_->IsSelectionToolActive())` call after `toolbar_->Render()` to propagate the toolbar state each frame.

## Design Decisions

- **Lights excluded from picking:** `GameLight` uses `BBox3::kInfinite` (–1e30 to +1e30). Including lights in the ray test would cause t = 0 (camera always inside), making the light permanently win. Filtering by `GameObjectType::kLight` is the cleanest fix.
- **`image_pos` vs `GetWindowPos()`:** `GetWindowPos()` returns the window outer top-left including the title bar. `GetCursorScreenPos()` before `ImGui::Image()` gives the exact pixel where the image starts, which is required for correct NDC ↔ screen mapping.
- **Alt guard for picking:** The camera controller uses Alt+LMB for orbit; plain LMB release would otherwise fire picking at the end of every orbit gesture. `!ImGui::GetIO().KeyAlt` prevents this false trigger.
- **`t_out` overload rather than modifying the original:** The existing `IntersectsRay(origin, dir)` is used in many geometric queries where the distance is not needed; adding an overload avoids breaking callers and keeps the simpler form as the default.
- **Clipping policy:** Edges with any endpoint behind the camera are skipped entirely rather than analytically clipped. For the editor use case this is sufficient; ImGui's scissor rect handles off-screen endpoints that are still in front of the camera.

## Follow-up / Notes for Next Features

- Issue #174 (toolbar) will replace `IsSelectionToolActive() { return true; }` with real tool-state logic.
- A future transform gizmo (ImGuizmo::Manipulate) should be activated once an object is selected.
- The ObjectsPanel (issue #176) could drive `EditorScene::SetSelectedObject()` from the list view, so both viewport clicks and panel clicks update the same selection state.

## Skills / Instructions Used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style guide, one class per file, include paths from `src/`
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; no editor headers in game/renderer
