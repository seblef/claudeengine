# Vehicle Editor: Simplified Preview & Wheel Selector (Issue #742)

## Summary

Follow-up to #717 and #740. Removed three redundant `MeshPreview` instances
(body 320×240, front wheel 64×64, rear wheel 64×64), replaced the DragFloat3
focus-to-activate gizmo workflow with an always-active gizmo bound to an
explicit wheel selector, and added click-to-select in the combined preview.

## Changes

### Files modified
- `src/editor/VehicleEditorWindow.h`
- `src/editor/VehicleEditorWindow.cpp`

### 1. Removed per-mesh previews

`body_preview_`, `front_wheel_thumb_`, and `rear_wheel_thumb_` members
(`std::unique_ptr<MeshPreview>`) were deleted from the header and their
construction removed from the constructor. All `SetTemplate()` call sites in
`UpdateBodyMesh`, `UpdateFrontWheelMesh`, `UpdateRearWheelMesh`, and `Open`
were removed. The `#include "editor/MeshPreview.h"` was dropped from the .cpp.

The Body section now shows a single-line mesh picker (label + Browse button)
with no thumbnail. The Wheels section no longer renders two 64×64 thumbs after
each picker. The 320×240 combined view is the only visual feedback.

### 2. Always-active wheel selector

`focused_wheel_` was changed from `-1` (none) to `0` (FL) as default and is
now always in `[0, 3]`. The `if (focused_wheel_ >= 0 && focused_wheel_ < 4)`
guard around the gizmo was replaced by an unconditional block so the gizmo
appears every frame.

A horizontal radio-button bar was added above the combined preview:

```
Active wheel:  (FL) (FR) (RL) (RR)
```

Clicking a radio button sets `focused_wheel_` immediately. The DragFloat3
rows below the preview still update `focused_wheel_` on focus/active (existing
behaviour) so numeric editing and visual editing stay in sync.

The `ImGui::IsMouseClicked` reset (`focused_wheel_ = -1`) was removed since
the selector always has one wheel active.

### 3. Click-to-select in the combined preview

`DrawCombinedPreview` now detects a click (mouse deactivated with
`MouseDragMaxDistanceSqr < 16.f` and `!ImGuizmo::IsUsing()`) and projects each
of the four wheel positions into screen space using the live view-projection
matrix:

```
clip = Vec4f(pos, 1) * GetViewProjectionMatrix()
sx = preview_x + (clip.x/clip.w * 0.5 + 0.5) * preview_width
sy = preview_y + (1 - (clip.y/clip.w * 0.5 + 0.5)) * preview_height
```

The wheel whose projected position is closest to the click and within 20 px is
selected. `core/Vec4f.h` was added to the includes for the projection.

The orbit drag was also guarded with `!ImGuizmo::IsUsing()` so dragging the
gizmo no longer orbits the camera simultaneously.

### 4. X-mirror preserved

`FL → FR` (x negation) and `RL → RR` mirroring in both the gizmo and
DragFloat3 paths remain unchanged.

## Decisions

- **No wheel highlight overlay**: the issue mentioned a coloured bounding-sphere
  overlay as an option; the gizmo handle itself is sufficient visual feedback
  and avoids adding a WireframeRenderer dependency here.
- **click threshold = 4 px / pick radius = 20 px**: these are the values
  suggested in the issue. They can be tuned as `kClickThreshSqr` and
  `kWheelPickThreshSqr` constants in the anonymous namespace.
- **Section headers kept**: the `Body` and `Wheels` `SeparatorText` headings
  were retained for layout clarity; the issue diagram does not require removing
  them.

## Skills / instructions referenced

- `impl-issue` skill
- `src/editor/CLAUDE.md`: one class per file, GUI vs edition logic separation,
  `cppcheck-suppress unusedStructMember` pattern
- `CLAUDE.md` (root): conventional commits, history file, cpplint before commit
