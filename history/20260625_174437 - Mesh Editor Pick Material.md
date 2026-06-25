# Mesh Editor — Pick Existing Material for Slot

**Issue**: #798  
**PR**: #799  
**Branch**: `feat/mesh-editor-pick-material`  
**Date**: 2026-06-25

## What was done

Added a **[Pick]** button next to the existing **[Edit]** button in the Materials table of `MeshEditorWindow`. The button lets the user assign any `.yaml` material already on disk to a slot in one click, without going through `MaterialEditorWindow`.

### Modified files

| File | Change |
|---|---|
| `src/editor/MeshEditorWindow.h` | Added `material_stems_`, `pick_popup_slot_`, `pick_popup_requested_` members; added `RenderPickMaterialPopup()` declaration |
| `src/editor/MeshEditorWindow.cpp` | Action column widened to 96 px; `[Pick]` button per slot; disk scan on open; `RenderPickMaterialPopup()` implementation; deferred `OpenPopup` in `Render()` |

## Key decisions

### Deferred `ImGui::OpenPopup` (critical)

The `[Pick]` button lives inside `ImGui::PushID(i)` (per-slot ID scope). Calling `ImGui::OpenPopup("##pick_mat")` from inside that scope hashes the popup ID including `i`, so it would never be found by `ImGui::BeginPopup("##pick_mat")` called at window level.

Fix: set `pick_popup_requested_ = true` inside the button handler (no `OpenPopup` there), then at the top of `Render()` after the main layout — but still inside the window — call `ImGui::OpenPopup` and then `RenderPickMaterialPopup()`. Both calls are now at the same ID-stack depth, so the popup ID matches.

### Disk scan on popup open (not every frame)

`data/materials/` is iterated only when the user clicks `[Pick]`, populating `material_stems_` once per popup session. This matches the acceptance criterion and avoids unnecessary filesystem I/O.

### `directory_iterator` vs `recursive_directory_iterator`

The issue specifies only direct children of `data/materials/` (no subdirectories for now). `std::filesystem::directory_iterator` (non-recursive) is used. Error code is passed to avoid exceptions.

### Popup rendered outside the table

`RenderPickMaterialPopup()` is called in `Render()` after `ImGui::EndTable()` so the popup is not clipped by the table's scissor region.

## What to keep in mind for next features

- The same popup pattern (deferred `OpenPopup` at window level) must be used any time a button inside `PushID` needs to open a popup that is rendered outside that scope.
- `material_stems_` is not invalidated when the user saves a new material via `[Edit]` in the same session. If that matters later, listen to `on_saved` callbacks to refresh the list.
- The Action column is now 96 px. If a third button is ever needed, widen it further or collapse into a `+` menu.

## Skills and CLAUDE.md instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md`: one class per file pair; ImGui calls bracketed by Begin/End; value members need full includes; prefer `std::find_if` over raw loops
- `CLAUDE.md` (root): conventional commits; history file required; cpplint before commit
