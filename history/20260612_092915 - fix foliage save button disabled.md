# Fix: Save button disabled when modifying foliage (issue #501)

## Problem

When painting foliage on the terrain in the editor, the Save button
remained greyed out even though the foliage data had changed.

## Root cause

The Save button's enabled state is driven by `scene_dirty_` in
`EditorWindow`. This flag is set in two ways:

1. Via `EditorCommandHistory::SetOnDirty` — fires whenever a command is
   pushed to the undo/redo stack (sculpt, paint, object transforms, …).
2. Explicitly in specific `EditorWindow` methods (environment panel,
   object placement, terrain creation, …).

Foliage brush strokes, layer additions/removals, and layer-settings edits
in `TerrainEditorPanel` never push to the command history and had no other
mechanism to signal dirtiness. As a result `scene_dirty_` was never set
and the Save button stayed disabled.

## Fix

Added a `SetOnFoliageModified(std::function<void()>)` callback to
`TerrainEditorPanel` (analogous to the existing `SetOnCreateFromImport`).
The callback is fired from:

- `OnBrushEnd()` — after a completed foliage paint/erase stroke.
- `RenderFoliageTab()` — when a layer is added or removed, when the layer
  name is edited, when a mesh or texture path is picked via the file
  dialog, and when any numeric property (spacing, scale, cull/billboard
  distance) is committed after a drag.

`EditorWindow::WireTerrainPanel()` sets this callback to
`scene_dirty_ = true`, exactly as other dirty sources do.

## Design decisions

- **No undo/redo for foliage** — the issue only asks for the Save button
  to be enabled; adding command-history support for foliage density maps
  would require significant additional work and is out of scope.
- **`IsItemDeactivatedAfterEdit()`** is used for `DragFloat` widgets so
  that `scene_dirty_` is set once at the end of a drag, not every frame
  while the user is dragging. This avoids spurious repeated fires.
- The callback is only registered when a terrain is wired
  (`WireTerrainPanel()`), so it is automatically cleared when the terrain
  is removed (the panel context is reset to all-nulls).

## Files changed

- `src/editor/TerrainEditorPanel.h` — added `SetOnFoliageModified()` and
  `on_foliage_modified_` member.
- `src/editor/TerrainEditorPanel.cpp` — fire the callback at all foliage
  mutation sites.
- `src/editor/EditorWindow.cpp` — register the callback in
  `WireTerrainPanel()`.

## Skills and CLAUDE.md rules applied

- Followed the editor module dependency invariant (only `editor` imports
  others, never the reverse).
- One class per `.h`/`.cpp` pair maintained.
- No new comments added beyond the callback doc-comment in the header.
- `cpplint` passes cleanly on all modified files.
