# Editor multi-selection (#579)

## Overview

Adds multi-object selection support to the editor. Users can now accumulate a
selection by holding Ctrl while clicking, and all editing operations (transform,
copy/paste, terrain alignment, delete) apply to the full selection.

## Changes

### EditorScene (selection model)

`selected_` (single raw pointer) replaced by `selection_` (`std::vector<game::
GameObject*>`).

New public API:
- `GetSelection()` — returns the full selection vector
- `IsSelected(obj)` — membership test
- `AddToSelection(obj)` / `RemoveFromSelection(obj)` / `ClearSelection()`
- `GetSelectedObject()` — backward-compatible: returns the single object when
  exactly one is selected, `nullptr` otherwise (keeps PropertiesPanel, WireframeRenderer, etc. working unchanged)
- `SetSelectedObject(obj)` — clears selection and sets a single object

`RemoveDynamicObject` and `ReclaimDynamicObject` now call `RemoveFromSelection`
instead of comparing against a single pointer, so deletion automatically updates
the full selection.

### PickingUtils

`PickObjectAt` gains a `ctrl_held = false` parameter:
- **Ctrl held + hit**: toggles the hit object in/out of the selection
- **Ctrl held + miss**: selection unchanged
- **No Ctrl**: replaces selection with hit (or clears on miss)

`DrawSelectedBBox` updated to draw an orange wireframe bbox for **each**
selected object, iterating over `GetSelection()`.

### SelectionTool

- **Ctrl-click** forwarded to `PickObjectAt` via `ImGui::GetIO().KeyCtrl`.
- **Delete key**: iterates over the full selection, pushing one
  `DeleteObjectCommand` per dynamic object.

### TransformTool + MultiTransformCommand

Single-object path is unchanged (gizmo placed at the object's own transform).

Multi-object path (2+ selected):
1. Pivot matrix = `T(centre)` where centre is the combined-bbox centre.
2. Each frame the pivot is updated by ImGuizmo, producing `pivot_after`.
3. All selected objects are updated: `new_T[i] = pivot_after * T(-centre) * T_before[i]`
   - Translation: adds the same world-space delta to each object.
   - Rotation (WORLD mode): rotates positions and orientations around the centre.
   - Scale: scales positions and extents from the centre.
4. On drag-end a single `MultiTransformCommand` is pushed, so Ctrl+Z undoes
   the whole group in one step.

New `MultiTransformCommand` stores a `vector<Entry{obj, before, after}>` and
applies/reverses all transforms together.

### DeleteObjectCommand

`was_selected_` now uses `scene->IsSelected(obj)` instead of comparing against
`GetSelectedObject()`. Execute/Redo rely on `ReclaimDynamicObject` to remove
the object from the selection (no more explicit `SetSelectedObject(nullptr)`,
which would have wiped remaining selected objects). Undo calls
`AddToSelection` instead of `SetSelectedObject` to restore the object into a
potentially multi-object selection.

### EditorWindow

- `clipboard_` changed from `unique_ptr<GameObject>` to
  `vector<unique_ptr<GameObject>>`.
- `CopySelectedObject()`: copies all selected copyable objects.
- `PasteObject()`: clears selection then places all clipboard clones (each as
  its own `PlaceObjectCommand`, which re-selects individually).
- `FallToTerrain()`: iterates over selection, applies each object independently.
- `CenterCameraOnObject()`: frames the combined bbox of all selected objects.
- Toolbar `can_copy`, `can_fall`, `can_center` logic updated to consider the
  full selection via `GetSelection()`.

### ObjectsPanel

`is_selected` now uses `scene.IsSelected(obj)` so all members of a
multi-selection are highlighted in the Objects panel list.

## Decisions

**No batch undo for Delete**: multiple `DeleteObjectCommand`s are pushed
individually. The user undoes one deletion at a time with Ctrl+Z. A composite-
command wrapper was not introduced to keep scope minimal; it can be added later.

**Paste clears then re-selects**: `PasteObject` calls `ClearSelection()` before
placing clones so that `PlaceObjectCommand::Execute` selects each pasted object,
leaving the full paste set selected after the operation.

**WireframeRenderer / PlayerStartGizmos unchanged**: both receive
`GetSelectedObject()`, which returns `nullptr` for multi-select. This means no
wireframe highlight or player-start gizmo when >1 object is selected — acceptable
UX given the bbox overlay already shows the selection.

## Output to keep in mind

- `GetSelectedObject()` is now semantically "single-selection accessor"; callers
  that legitimately need all selected objects must use `GetSelection()`.
- The `pivot_after * T(-centre) * T_before[i]` formula relies on the gizmo
  starting at `T(centre)` (identity rotation/scale). If a future ROTATE_SCREEN
  or LOCAL mode is added the same formula should still hold because the pivot
  is always pure-translation before manipulation starts.
- Skills used: `impl-issue`
- Key instructions followed: editor CLAUDE.md separation of GUI vs logic;
  Google C++ style; one class per file; `[[nodiscard]]` on const getters.
