# Allow selection with transform tools (issue #505)

## Summary

When a transform tool (translate, rotate, scale) was active, object picking was entirely disabled. The user could not select another object or deselect the current one by clicking outside the gizmo. This fix enables picking for transform tools while keeping gizmo interaction intact.

## Changes

### `src/editor/EditorTool.h`
Added `IsTransformTool(EditorTool)` helper, symmetric to the existing `IsCreationTool` helper, returning true for `kTranslate`, `kRotate`, and `kScale`.

### `src/editor/EditorWindow.cpp`
Changed the `SetSelectionActive` call (line ~184) to also enable picking for transform tools:
```cpp
viewport_->SetSelectionActive(active_tool == EditorTool::kSelection ||
                              IsTransformTool(active_tool));
```
Previously only `kSelection` enabled picking; creation tools correctly remained disabled.

### `src/editor/EditorViewport.cpp`
Added `!gizmo_was_using_` to the picking condition in `Render()`.

**Why this is necessary:** the picking block runs _before_ the ImGuizmo manipulation block that updates `gizmo_was_using_`. On the frame the user releases the mouse after a gizmo drag, `IsMouseReleased` fires and `IsOver()` may be false (cursor moved away from handle), so without this guard the drag-end release would spuriously trigger `PickObjectAt` and potentially change the selection.

`gizmo_was_using_` still holds the previous-frame value (`true`) when the picking block runs, so the condition `!gizmo_was_using_` correctly blocks the pick on that one frame.

## Decision rationale

- `!ImGuizmo::IsOver()` protects against gizmo-handle clicks (cursor on handle → IsOver true → pick suppressed).
- `!gizmo_was_using_` protects against drag-end releases (gizmo being dragged → was_using true from prev frame → pick suppressed on release frame).
- Together these two guards mean picking only fires when the user genuinely clicks on empty space or on another object while a transform tool is active — matching the issue spec exactly.

## Notes for future contributions

- The `IsTransformTool` helper can be reused if other subsystems need to distinguish transform tools from selection/camera/creation tools.
- `gizmo_was_using_` is updated at the end of the gizmo block; any code that needs to react to drag-end should check it before the gizmo block runs, or track its own copy.

## Skills and CLAUDE.md instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style guide, one class per file, include root `src/`
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph; ImGui/ImGuizmo usage conventions
- Root `CLAUDE.md`: conventional commits, branch naming, history file, cpplint before commit
