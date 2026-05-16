# Click-to-place lights (OmniLight, CircleSpotLight, RectangleSpotLight)

**Date:** 2026-05-16
**Issue:** #221
**Branch:** feat/click-to-place-lights

## Summary

When any of the three light-creation toolbar tools (`kCreateOmniLight`,
`kCreateCircleSpot`, `kCreateRectSpot`) is active, the next LMB click in the
viewport places a new light at the y=0 floor-plane intersection and reverts to
the selection tool. No modal or preview object is needed; the light uses
`GameLightDesc` defaults and can be tuned in the Properties Panel afterwards.

## Changes

### `src/editor/EditorViewport.h`
- Added a forward declaration for `renderer::LightType` (scoped enum,
  avoids a full include in the header).
- Declared the private method `PlaceLightAt(mouse_pos, image_pos, image_size,
  type)`.

### `src/editor/EditorViewport.cpp`
- Added `ToolToLightType(EditorTool)` helper (anonymous namespace) that maps
  the three light creation tools to their `renderer::LightType`; returns
  `std::nullopt` for all other tools.
- Implemented `EditorViewport::PlaceLightAt`: same ray→y=0 floor-plane formula
  reused from `PlaceMeshAt`; creates `game::GameLight(type)` with default
  `GameLightDesc`, sets its world transform to the hit point, adds it to the
  scene, selects it, and fires `on_placement_done_` to revert the toolbar to
  `kSelection`.
- In `Render()`, inserted a "light placement mode" block between the mesh
  placement block and the object picking block: when the active tool resolves
  to a `LightType` and the window is hovered, sets the cursor to
  `ImGuiMouseCursor_ResizeAll` (the crosshair-style move cursor — this version
  of ImGui does not provide `ImGuiMouseCursor_Crosshair`) and on LMB release
  (without Alt) calls `PlaceLightAt`.

### `src/editor/EditorWindow.cpp`
- Removed the NYI log message for non-mesh creation tools. Light tools now
  work purely via the viewport, with no modal.

## Design decisions

### No preview for lights
Unlike mesh placement (which shows a preview object that follows the cursor),
light placement fires on a single click. This matches the issue spec and keeps
the implementation minimal: lights have no visual geometry to preview, only a
wireframe overlay that is already drawn every frame.

### Cursor choice
The issue spec references `ImGuiMouseCursor_Crosshair`, which does not exist in
the vendored ImGui version. `ImGuiMouseCursor_ResizeAll` (the four-direction
move arrow) is the closest visual signal available and is the same hint used
by many DCC tools for "click to place here".

### Reuse of `on_placement_done_` callback
The existing callback (set by `EditorWindow`) already reverts the toolbar to
`kSelection` and clears `placement_active_`. Light placement fires the same
callback so tool-reversion is handled uniformly.

## Skills files used
- `impl-issue`

## Instructions especially followed
- `src/CLAUDE.md`: one class per `.h`/`.cpp` pair; Google C++ style guide;
  include root is `src/`.
- `src/editor/CLAUDE.md`: one class per file pair; all ImGui calls within
  `NewFrame`/`Render` brackets; no back-pointer from `EditorViewport` to
  `EditorWindow` (callback pattern retained).
- Root `CLAUDE.md`: no unnecessary abstractions; no comments explaining *what*
  the code does.

## Output to keep in mind for future features

- `ImGuiMouseCursor_Crosshair` is missing from this ImGui build;
  `ImGuiMouseCursor_ResizeAll` is the closest substitute.
- The `on_placement_done_` callback is the canonical way for
  `EditorViewport` to signal tool completion back to `EditorWindow`.
- The ray→y=0 floor intersection formula is duplicated in
  `UpdatePreviewPosition`, `PlaceMeshAt`, and `PlaceLightAt`. If a fourth
  placement method is added, extract it into a private helper.
