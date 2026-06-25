# Dimension Overlay — Size Arrows in Mesh/Vehicle Preview and Viewport Selection

## Summary

Added real-world size readouts (in metres) to three editor surfaces:
1. **Mesh preview** (`MeshPreview`) — coloured double-sided arrows + labels on the ImGui image.
2. **Vehicle preview** (`VehicleEditorWindow`) — same overlay on the combined body+wheels preview.
3. **Viewport selection** (`SelectionTool`, `TransformTool`) — dimension annotations when one or more objects are selected.

All three share a single utility function for visual consistency.

---

## Files changed

| File | Change |
|---|---|
| `src/editor/tools/PickingUtils.h` | Added `DrawBBoxDimensionOverlay` and `DrawSelectedBBoxDimensions` declarations; added `core/BBox3.h` and `core/Mat4f.h` includes |
| `src/editor/tools/PickingUtils.cpp` | Implemented both functions; added `<cstdio>` include |
| `src/editor/MeshPreview.cpp` | Include `PickingUtils.h`; call `DrawBBoxDimensionOverlay` after `AddImage` |
| `src/editor/VehicleEditorWindow.cpp` | Include `PickingUtils.h`; call `DrawBBoxDimensionOverlay` after `AddImage` in `DrawCombinedPreview` using world-space bboxes |
| `src/editor/tools/SelectionTool.cpp` | Call `DrawSelectedBBoxDimensions` after `DrawSelectedBBox` |
| `src/editor/tools/TransformTool.cpp` | Same |

---

## Design decisions

### `DrawBBoxDimensionOverlay` algorithm

- For each of X (red), Y (green), Z (blue): project the two face-centre points of the bbox through the VP matrix to screen space.
- Compute a perpendicular offset outward from the projected bbox centre (12 px) to avoid clipping through the mesh silhouette.
- Draw: a 1.5 px line, filled triangle arrowheads (8 px long, 5 px base) at each endpoint, and a centred `"X.XX m"` label.
- Axes whose projected length is < 8 px are suppressed.

### Vehicle preview uses world-space bboxes

The issue spec suggested `GetLocalBBox()` for all instances but that would miss wheel position offsets. Since wheel `MeshInstance` objects have their world matrices set to `WheelMat(position, mirrored)`, using `GetWorldBBox()` correctly includes each wheel's authored position in the combined bbox.

### Viewport selection uses world-space bboxes

`DrawSelectedBBoxDimensions` uses `obj->GetWorldBBox()` so the displayed dimensions reflect the object's actual scale in the scene (not just the template's local mesh dimensions).

### VP matrix for previews

In both MeshPreview and VehicleEditorWindow, the preview renders objects at identity (or wheel) transform in the preview camera's world. The VP is `projection * view` from the respective preview camera, which maps directly from "preview world" to clip space — no extra model transform needed.

---

## Skills and CLAUDE.md instructions followed

- **Skills used**: `impl-issue`
- **Guidelines followed**:
  - Checked out `dev` and pulled before branching.
  - Branch prefixed `feat/`.
  - Ran `cpplint` before committing — fixed one `whitespace/newline` warning (braced compound statement on same line as `if`).
  - Conventional commit message.
  - PR targets `dev` with `Close #772`.
  - One class per `.h`/`.cpp` pair (utility functions added to existing `PickingUtils` pair, not a new file).
  - `std::find_if` / STL algorithm preference not applicable here (no index loops flagged).

---

## Output to keep in mind

- `DrawBBoxDimensionOverlay` is now a public function in `PickingUtils.h` — future editor features (e.g., snapping helpers, measurement tool) can reuse it directly.
- The 8 px minimum projected length threshold avoids label clutter when the camera is nearly perpendicular to an axis.
- The outward-offset direction is computed from the projected bbox centre, so arrows correctly flip when the camera is behind the mesh.
