# Transform Gizmo (ImGuizmo) for Selected Object

**Issue:** #173  
**Branch:** `feat/transform-gizmo`  
**Date:** 2026-05-15

## Summary

Adds an interactive ImGuizmo transform gizmo over the selected scene object. The active operation (translate / rotate / scale) is driven by the active toolbar tool. Selection and camera tools suppress the gizmo.

## Changes

### `src/editor/EditorTool.h` (new)
Defines the `EditorTool` enum with five values: `kSelection`, `kCamera`, `kTranslate`, `kRotate`, `kScale`. Placed in its own header so both `EditorToolbar` and `EditorViewport` can include it without creating a circular dependency.

### `src/editor/EditorToolbar.h`
- Includes `EditorTool.h`.
- Adds `active_tool_` field (default `kSelection`).
- Adds `GetActiveTool()` returning `active_tool_`.
- `IsSelectionToolActive()` now delegates to `active_tool_ == EditorTool::kSelection` instead of hardcoding `true` (behaviour is identical until #174 changes the default).

### `src/editor/EditorViewport.h`
- Includes `EditorTool.h`.
- Adds `SetActiveTool(EditorTool)` public method.
- Adds `active_tool_` private field (default `kSelection`).

### `src/editor/EditorViewport.cpp`
**`ToolToImGuizmoOp` helper (anonymous namespace):**  
Maps `EditorTool → std::optional<ImGuizmo::OPERATION>`. Returns `nullopt` for `kSelection` and `kCamera`, signalling no gizmo should be shown.

**Gizmo rendering in `Render()`:**  
Placed after `ImGuizmo::SetRect()` + matrix setup, before `ViewManipulate`, so the gizmo renders below the XYZ overlay. Reuses the already-transposed `view_im` / `proj_im` arrays.

1. Transposes the selected object's world transform (our column-vector `Mat4f` → ImGuizmo's row-vector convention).
2. Calls `ImGuizmo::Manipulate(view_im, proj_im, op, WORLD, model_im)`.
3. On `ImGuizmo::IsUsing()`, transposes the written-back `model_im` and calls `SetWorldTransform()`.

**Picking guard updated:**  
Added `!ImGuizmo::IsOver()` to the LMB-release picking condition so clicking on a gizmo handle does not trigger an unintended object deselect/reselect.

### `src/editor/EditorWindow.cpp`
Added `viewport_->SetActiveTool(toolbar_->GetActiveTool())` after the existing `SetSelectionActive` call.

## Design Decisions

- **`EditorTool` in its own header:** Both toolbar and viewport need the enum. Putting it in either class header would create an include dependency from viewport→toolbar or vice versa, violating the module structure. A standalone enum header avoids this.
- **`std::optional<ImGuizmo::OPERATION>` for the mapping:** Cleaner than a boolean flag + separate operation field; a single function covers both the "show gizmo?" and "which op?" questions.
- **Matrix transposition pattern:** ImGuizmo uses row-vector convention (translation in last row); our `Mat4f` uses column-vector convention (translation in last column). The same transpose-before / transpose-after pattern already used for `ViewManipulate` is applied to `Manipulate`.
- **`ImGuizmo::IsOver()` for picking guard:** Prevents an LMB click on a gizmo axis from being mistakenly interpreted as a "click on empty space → deselect". `IsUsing()` alone is insufficient because the gizmo can be hovered without being dragged.
- **Gizmo before ViewManipulate:** The XYZ overlay must render on top of the gizmo, so `Manipulate` is called first.
- **`ImGuizmo::BeginFrame()` already present:** Called in `EditorSystem::Run()` each frame; no duplicate call needed in `EditorViewport::Render()`.

## Follow-up / Notes for Next Features

- Issue #174 (toolbar) will add clickable tool buttons and update `active_tool_` in `EditorToolbar`, which will automatically propagate to the viewport via `SetActiveTool`.
- A future keyboard shortcut layer (W/E/R for translate/rotate/scale) can simply call `SetActiveTool` on the toolbar or viewport directly.
- `ImGuizmo::LOCAL` mode (gizmo aligned to the object's local axes) could be exposed as a toggle alongside the mode buttons.

## Skills / Instructions Used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, include paths from `src/`
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph
