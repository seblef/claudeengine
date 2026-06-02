# Improved Terrain Import Menu

**Issue**: #369
**Branch**: feat/editor-improved-terrain-import-369

## Summary

Moved terrain import functionality from the terrain editor panel to the main
Terrain menu, and added a terrain removal flow with confirmation.

## Changes

### `src/editor/TerrainEditorPanel.h` / `.cpp`

- Added `OpenImportWindow()` public method: sets `show_import_window_ = true` and
  clears the status message.
- Added `RenderImportWindow()` public method: renders a standalone floating ImGui
  window ("Terrain Import") when `show_import_window_` is true. Contains the
  resize-confirmation modal, status message, height-range controls, and the
  PNG / HDR import buttons — everything that was previously in the Import section
  of the "Import / Export" tab.
- Added `RenderImportStatusAndModal()` private helper: extracted the modal and
  status-message rendering so it can be called from `RenderImportWindow()`.
- Renamed `RenderImportExportTab()` → `RenderExportTab()`: the tab now only
  contains the export section (PNG and R16). Status messages from export
  operations are still displayed at the top of this tab.
- Renamed `ActiveTab::kImportExport` → `ActiveTab::kExport` to match.
- Renamed the tab label "Import / Export" → "Export".
- Added `show_import_window_` bool member (default `false`).

### `src/editor/EditorWindow.h` / `.cpp`

- Added "Import" item to the Terrain menu (disabled when no terrain exists).
  Calls `terrain_panel_.OpenImportWindow()`.
- Added "Remove" item to the Terrain menu (disabled when no terrain exists).
  Sets `confirm_remove_terrain_ = true`, which triggers
  `"Remove Terrain?##modal"` on the next frame.
- Added `RemoveTerrain()` private method: calls
  `scene_->RemoveDynamicObject(gt)`, resets `terrain_normal_map_`, clears the
  command history, closes the sculpt panel, and calls `WireTerrainPanel()`.
- Added `confirm_remove_terrain_` bool member for modal coordination.
- `terrain_panel_.RenderImportWindow()` is called each frame outside any
  Begin/End block (step 11a in the render sequence).

## Decisions & Rationale

- **Floating window vs. docked panel**: import is a one-shot operation; a
  floating window (`ImGuiWindowFlags_AlwaysAutoResize`) is more ergonomic than
  forcing the user to navigate a tab inside the sculpt panel.
- **Resize modal stays in the import window**: it is triggered by import
  operations and is rendered via `RenderImportStatusAndModal()` inside the
  import window's `Begin/End` block, keeping all import state self-contained.
- **History cleared on Remove**: removing the terrain invalidates all undo
  entries that reference terrain data pointers; clearing is the safest option.
- **No undo for Remove**: terrain removal is a destructive operation not
  currently tracked by the command system; the confirmation dialog makes this
  explicit.

## Skills / Instructions Used

- `CLAUDE.md`: branch from `dev`, conventional commit, PR to `dev`, history file.
- `src/CLAUDE.md`: one class per file, Google C++ style, include paths from `src/`.
- `src/editor/CLAUDE.md`: one class per `.h/.cpp` pair, ImGui calls bracketed.

## Output to Keep in Mind

- `terrain_panel_.RenderImportWindow()` must be called **outside** any
  `ImGui::Begin/End` block; it owns its own window context.
- If a future feature needs to track terrain removal in the undo history, a
  `RemoveTerrainCommand` would need to store the `GameTerrain` unique_ptr and
  the normal map, then re-wire the panel on undo/redo.
