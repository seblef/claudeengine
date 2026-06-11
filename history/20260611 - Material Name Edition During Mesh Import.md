# Material Name Edition During Mesh Import

**Issue:** #468  
**Branch:** feat/editor-material-name-edit-on-import-468  
**Date:** 2026-06-11

## Problem

Material names coming from imported mesh files (FBX, OBJ) or from existing `.emesh` files
are read-only in the Mesh Editor Window.  When the source uses generic or auto-generated
names (e.g. `slot_0`, `Material.001`) users have no way to rename them before committing
the mesh, leading to naming conflicts and meaningless material references in the emesh.

## Changes

### `src/editor/MeshEditorWindow.h`

Added a `char name_buf[256]` field to the private `MatSlot` struct.  This buffer is the
editable, per-slot name the user interacts with.  It is kept in sync with
`saved_material_path` so the emesh always references the current user-chosen name.

### `src/editor/MeshEditorWindow.cpp`

**`LoadSourceFile()` / `LoadEmeshFile()`**  
Changed the submesh iteration from range-for to an index loop so each slot can be
initialised with a fallback name (`"slot_N"` when `material_name` is empty).
`name_buf` is initialised from the effective slot name via `std::strncpy`.

**`RenderMaterialSlots()`**  
Replaced the static `TextDisabled` / `TextUnformatted` display with an
`ImGui::InputText("##name", slot.name_buf, …)`.  On any edit:
- `saved_material_path` is updated to `"materials/<new_name>.yaml"` immediately, so
  `BuildTransformedMesh()` picks up the correct material reference on commit.
- `preview_dirty_` is set to rebuild the live preview.

The slot is now declared `auto&` (non-const) to allow mutations.

When the user saves inside `MaterialEditorWindow` the existing `on_saved` callback now
also syncs `name_buf` with the actually-saved material name so the two fields stay
consistent.

**`OpenWithDesc` call**  
`desc.slot_name` now uses the current `name_buf` content instead of `original_name`, so
the Material Editor opens with the user-chosen name pre-filled.

## Design decisions

- **Inline InputText instead of a separate rename modal** — The slot table already had a
  dedicated "Name" column.  Making it directly editable is the most discoverable UX and
  adds no extra chrome.  A modal would be overkill for a simple rename.
- **Live update of `saved_material_path`** — `saved_material_path` is the single source of
  truth consumed by `BuildTransformedMesh()`.  Keeping it in sync on every keystroke means
  the user can rename and immediately commit without a separate "Apply" step.
- **Fallback `"slot_N"` name** — Matches the existing behaviour of the "Edit" button which
  used the same fallback.  Now it is applied at load time so the buffer is never empty.
- **Sync `name_buf` from `on_saved`** — If the user opens MaterialEditorWindow and saves
  under a different name, `name_buf` is updated to match.  This prevents the two fields
  drifting apart.

## Notes for future work

- Validation (trim whitespace, reject empty string) is not enforced on the InputText.
  The empty-string guard in the `InputText` callback prevents an empty `saved_material_path`
  but the buffer itself could contain only spaces.  A future pass could add a red border.
- The "Action" column still only contains the "Edit" button.  A future enhancement could
  add a "Browse" button to pick an existing material file as the slot reference.

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, include root is `src/`, Google C++ style guide
- `src/editor/CLAUDE.md`: one class per `.h`/`.cpp` pair, ImGui calls within frame bracket
- Root `CLAUDE.md`: branch from `dev`, cpplint, conventional commits, history file
