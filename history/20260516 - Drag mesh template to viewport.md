# Drag mesh template from ResourcesPanel to viewport

**Date:** 2026-05-16
**Issue:** #220
**Branch:** feat/drag-mesh-template-to-viewport

## Summary

Lets the user drag a mesh template leaf from the Resources panel and drop it anywhere in the viewport to place a `GameMesh` directly, without opening the `MeshSelectionModal`.

## Changes

### `src/editor/ResourcesPanel.cpp`

After each `ImGui::TreeNodeEx(label, kLeafFlags)` call for mesh templates, a `BeginDragDropSource` block was added. It emits a `"MESH_TEMPLATE"` payload carrying a raw `MeshTemplate*`. `ImGuiDragDropFlags_SourceAllowNullID` is passed so the drag gesture is recognised even when the leaf item does not push an ID onto the ImGui stack (due to `ImGuiTreeNodeFlags_NoTreePushOnOpen`). ImGui renders the drag ghost text as `"Place: <name>"`.

### `src/editor/EditorViewport.h`

Declared the new private method:
```cpp
void PlaceMeshAt(ImVec2 mouse_pos, ImVec2 image_pos, ImVec2 image_size,
                 game::MeshTemplate* tmpl);
```

### `src/editor/EditorViewport.cpp`

**Drop target** — inserted immediately after the `ImGui::Image(...)` call (which marks the last item, making `BeginDragDropTarget` attach to the rendered image rect). On payload acceptance the drop handler calls `PlaceMeshAt` with the payload template pointer and the current mouse position.

**`PlaceMeshAt`** — mirrors the ray-floor intersection logic from `UpdatePreviewPosition` / `PlaceMesh` (issue #219), but in a single synchronous function:
1. Unprojects `mouse_pos` through the inverse VP matrix to obtain a world-space ray.
2. Intersects the ray with the `y=0` floor plane.
3. Creates a `GameMesh`, adds it via `AddDynamicObject`, sets its world transform to the hit point, and selects it.

No tool change is performed (the active tool remains `kSelection`), and no preview object lifecycle is involved.

## Design decisions

- **No `on_placement_done_` callback invoked** — the drop completes in-place with the tool unchanged, so no toolbar state reset is needed. This is the correct behaviour: the drag-and-drop flow is independent of the click-to-place modal flow.
- **Pointer stability** — `MeshTemplate` uses ref-counting (`core::Resource`) and is kept alive by the scene's `mesh_templates_` vector for the lifetime of the editor session. Carrying a raw pointer in the drag payload is safe.
- **`SourceAllowNullID`** — required because `ImGuiTreeNodeFlags_NoTreePushOnOpen` prevents the leaf from pushing a stack ID. Without this flag ImGui would silently skip `BeginDragDropSource`.
- **Drop target scope** — `BeginDragDropTarget` is called only when `scene_` is non-null, so the guard mirrors the existing `pending_mesh_template_` guard.
- **No change to `ResourcesPanel`'s interface** — the panel carries no `EditorScene` reference; the payload carries the template, and only the viewport owns the placement logic.

## Output to keep in mind for future features

- The `"MESH_TEMPLATE"` drag-drop payload key is now a de-facto protocol between `ResourcesPanel` and `EditorViewport`. If other drop targets (e.g. a scene tree panel) need to accept the same payload, reuse the same string constant.
- `PlaceMeshAt` duplicates the ray-unprojection logic that already exists in `PickObjectAt` and `UpdatePreviewPosition`. A future refactor could extract a shared `UnprojectRay(ImVec2, ImVec2, ImVec2) → std::optional<core::Vec3f>` free function to reduce duplication.
- The floor is always at `y=0`. If the scene ever supports a configurable floor height, both `PlaceMeshAt` and `UpdatePreviewPosition` must be updated.

## Skills and instructions followed

- `impl-issue` skill
- `CLAUDE.md` (root): Git workflow (checkout dev → pull → branch → implement → cpplint → commit → PR), history file requirement, conventional commits
- `src/CLAUDE.md`: one class per `.h`/`.cpp`, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md`: one class per file pair, all ImGui calls bracketed correctly, editor is the leaf of the dependency graph
