# Editor: Object Transform Edit in Properties Panel

**Date**: 2026-06-15
**Issue**: #522
**Branch**: `feat/editor-transform-properties-panel-522`

## Summary

Added position, rotation and scale widgets to the object properties panel, allowing
direct numerical editing of any selected object's world transform.

## Changes

### `src/editor/PropertiesPanel.h`
- Added `#include "core/Mat4f.h"` (stores `before_transform_` snapshot).
- Added `SetOnTransformChanged(std::function<void(game::GameObject*)>)` setter — called
  whenever the transform is mutated from the panel so callers can update spatial
  acceleration structures.
- Added private method `RenderTransformSection(game::GameObject*)`.
- Added private members `before_transform_` (Mat4f) and `on_transform_changed_` (callback).

### `src/editor/PropertiesPanel.cpp`
- Added `#include <ImGuizmo.h>`, `#include "core/Mat4f.h"`, and
  `#include "editor/commands/TransformCommand.h"`.
- Implemented `RenderTransformSection`: decomposes the world matrix via
  `ImGuizmo::DecomposeMatrixToComponents`, shows three `DragFloat3` widgets
  (Position / Rotation / Scale), recomposes on change via
  `ImGuizmo::RecomposeMatrixFromComponents`, and pushes a `TransformCommand` for
  undo/redo on drag end (matching the before-snapshot/after pattern used by
  `RenderLightProperties`).
- `Render()` now calls `RenderTransformSection` between the Name field and the
  type-specific section.

### `src/editor/EditorViewport.h` / `.cpp`
- Added `UpdateMovedObject(game::GameObject*)` — thin wrapper around
  `picking_acc_.UpdateMoved(obj)` so the picking grid stays consistent when an
  object is repositioned from the properties panel.

### `src/editor/EditorWindow.cpp`
- Wired `properties_panel_->SetOnTransformChanged` to call
  `viewport_->UpdateMovedObject(obj)`.

## Decisions

### ImGuizmo decompose / recompose
`ImGuizmo::DecomposeMatrixToComponents` and `RecomposeMatrixFromComponents` already
exist in the project (they are part of the ImGuizmo dependency used by `TransformTool`)
and give consistent Euler angles in degrees. The ImGuizmo convention is row-vector /
translation-in-last-row, so the world transform is transposed before passing to these
functions and transposed back after recompose — exactly the same pattern as `TransformTool`.

### Matrix convention note
The ImGuizmo header warns about numerical stability in decompose/recompose. In practice
this is fine for the discrete-edit use case (not continuous per-frame jitter like a gizmo
drag).

### Scale minimum
Scale is clamped to 0.001 as the widget minimum to prevent degenerate zero-scale matrices.

### Undo/redo
Same before/after snapshot approach as `RenderLightProperties`: `IsItemActivated()`
captures `before_transform_`, `IsItemDeactivatedAfterEdit()` pushes a `TransformCommand`.
Each drag widget produces one undoable command.

### Picking accelerator
The `PickingAccelerator` is owned by `EditorViewport` (not accessible to
`PropertiesPanel`). A callback pattern (`SetOnTransformChanged`) was chosen to keep
the panel decoupled from viewport internals, consistent with how particle-editor
opening is wired.

## Skills / CLAUDE.md Notes Used
- `src/editor/CLAUDE.md` — GUI vs. edition logic separation: panel reads widgets and
  calls setters; command push stays in the panel since no separate algorithm class is
  needed here (the transform is a single `SetWorldTransform` call).
- `src/CLAUDE.md` — one class per file, include root `src/`, Google C++ style.

## Output to Keep in Mind
- `TransformCommand.Undo()` / `Redo()` do not update the picking accelerator (pre-existing
  limitation; also true for gizmo-driven undo). If this becomes a problem, the command
  would need a pointer to `PickingAccelerator` and call `UpdateMoved` in Undo/Redo.
- ImGuizmo's Euler angles are ZYX extrinsic (same convention as `Mat4f::Rotation`),
  so gizmo-rotated objects and panel-rotated objects will show consistent angles.
