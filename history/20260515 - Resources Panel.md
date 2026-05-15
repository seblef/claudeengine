# Left Panel: Resources Tab — Template Resources Tree

**Issue:** #175  
**Branch:** `feat/resources-panel`  
**Date:** 2026-05-15

## Summary

Implements the Resources tab in the left Scene panel: a two-section tree showing all named materials and all file-backed mesh templates with coloured icon squares on the root nodes.

## Changes

### `src/core/Resource.h`
Added `static const std::map<TId, TDerived*>& GetAll()` to the `Resource<TId, TDerived>` template. This exposes the per-type static registry read-only, enabling derived classes to iterate all live instances without duplicating the storage.

### `src/game/MeshTemplate.h` / `.cpp`
Added `static std::map<std::string, MeshTemplate*> GetAll()`. It iterates `Resource::GetAll()`, skips procedural entries (key prefix `__proc_`), extracts the path basename, and returns a sorted name → pointer map. Added `<map>` to the header.

### `src/editor/EditorScene.h` / `.cpp`
- Added `std::map<std::string, renderer::Material*> materials_` (non-owning pointers).
- Added `GetMaterials()` and `AddMaterial()` public methods.
- `EditorScene.h` forward-declares `renderer::Material` (pointer-only use in the header); the full type is included in `.cpp`.
- The constructor registers the two startup materials: `"floor_grey"` (plane) and `"default"` (cube).

### `src/editor/ResourcesPanel.h`
Changed `Render()` signature to `Render(EditorScene& scene)`. Forward-declares `EditorScene` (no full include needed in the header).

### `src/editor/ResourcesPanel.cpp`
Full tree implementation:
- **Materials section**: 12×12 magenta `ColorButton` icon → `TreeNodeEx("Materials", DefaultOpen)` → one leaf per `scene.GetMaterials()` entry.
- **Meshes section**: 12×12 cyan `ColorButton` icon → `TreeNodeEx("Meshes", DefaultOpen)` → one leaf per `MeshTemplate::GetAll()` entry.
- Leaf flags: `ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen` (no expand arrow, no indentation push).
- Icon flags: `NoPicker | NoTooltip | NoDragDrop` making `ColorButton` display-only.

### `src/editor/EditorWindow.cpp`
Updated the `Render()` call from `resources_panel_->Render()` to `resources_panel_->Render(*scene_)`.

## Design Decisions

- **`Resource::GetAll()` instead of a separate registry in MeshTemplate**: The static map is already maintained by `Resource`; exposing it avoids a second data structure and a separate bookkeeping call on every load/release.
- **Forward declaration of `renderer::Material` in EditorScene.h**: The header only holds a `Material*` in a `std::map`; a forward declaration is sufficient and avoids pulling the full Material header into every file that includes EditorScene.
- **`AddMaterial()` public method**: Allows future code (import dialog, issue #16) to register materials without editing scene internals.
- **No interaction on leaf click**: Deferred to the Properties milestone as specified.

## Follow-up / Notes for Next Features

- Issue #16 (material import) will call `scene.AddMaterial(name, mat)` after loading.
- Issue #176 (Objects panel) follows a similar pattern; `EditorScene::GetObjects()` already returns the list.
- When selection wiring is added, clicking a leaf should set a selected-resource state, to be shown in the Properties panel.

## Skills / Instructions Used

- `impl-issue` skill  
- `src/CLAUDE.md`: one class per file, include paths from `src/`  
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph
