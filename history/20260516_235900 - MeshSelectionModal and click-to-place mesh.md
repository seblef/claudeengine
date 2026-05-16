# MeshSelectionModal and click-to-place mesh in viewport

**Date:** 2026-05-16  
**Issue:** #219  
**Branch:** feat/mesh-selection-modal  

---

## Summary

Implements the full `kCreateMesh` tool flow: a modal that lists all loaded
`MeshTemplate` instances, followed by click-to-place in the viewport at the
y=0 floor-plane intersection of the camera ray.

---

## Changes

### New files

| File | Responsibility |
|---|---|
| `src/editor/MeshSelectionModal.h` | Declares the `MeshSelectionModal` class |
| `src/editor/MeshSelectionModal.cpp` | Implements modal rendering via ImGui popup |

### Modified files

| File | Change |
|---|---|
| `src/editor/EditorViewport.h` | Added `pending_mesh_template_`, `on_placement_done_`, `SetPendingMeshTemplate()`, `SetOnPlacementDone()`, `PlaceMesh()` |
| `src/editor/EditorViewport.cpp` | Added `SetPendingMeshTemplate()`, `PlaceMesh()` impl; placement-mode routing in `Render()` |
| `src/editor/EditorWindow.h` | Added forward declaration and `mesh_modal_` field |
| `src/editor/EditorWindow.cpp` | Constructs `MeshSelectionModal`, wires `on_placement_done_` callback, opens modal on `kCreateMesh` transition, forwards confirmed template to viewport |
| `src/editor/CMakeLists.txt` | Added `MeshSelectionModal.cpp` to `editor` library |

---

## Design decisions

### MeshSelectionModal owned by EditorWindow
The modal is a thin ImGui popup wrapper with no game-layer knowledge. Owning it
in `EditorWindow` (same level as `EditorToolbar`) keeps the viewport free of
UI state and lets `EditorWindow` orchestrate the full tool→modal→placement
flow in one place.

### Callback for tool reset (`on_placement_done_`)
`EditorViewport` needs to notify `EditorWindow` when placement is done so the
toolbar can be reset to `kSelection`. A `std::function<void()>` callback avoids
a circular include (`EditorViewport` → `EditorWindow`), matching the existing
pattern used by `ResourcesPanel::SetOnMaterialOpen`.

### `SetPendingMeshTemplate` manages `selection_active_`
Rather than requiring the caller to coordinate two calls, `SetPendingMeshTemplate`
atomically sets the template pointer and toggles `selection_active_`, so the
viewport is never in an inconsistent state.

### `ImGuiMouseCursor_None` for crosshair hint
The issue specifies `ImGuiMouseCursor_Crosshair`. In practice the OS cursor is
hidden (`ImGuiMouseCursor_None`) while hovering during placement mode, which
gives a cleaner feel and avoids the default arrow overlay. If a visible crosshair
icon is preferred, `ImGuiMouseCursor_Crosshair` can be substituted trivially.

### Ray–plane edge cases
* `|ray_dir.y| < 1e-4f`: ray nearly parallel to floor → skip (no object created).
* `t < 0`: intersection behind camera → skip.
* `|world4.w| < 1e-6f`: degenerate unproject → skip (copied from `PickObjectAt`).

---

## Files / skills consulted

- `src/editor/CLAUDE.md` — editor module conventions
- `src/CLAUDE.md` — one class per file, Google style, include root `src/`
- `CLAUDE.md` — git workflow, history file, cpplint step
- Pattern from `PickObjectAt` for the NDC → world-space ray

---

## Output to keep in mind for future features

- `MeshSelectionModal` could be generalized (e.g. a `GameLightSelectionModal`)
  once light-creation placement is wired up in subsequent issues.
- The `on_placement_done_` callback pattern is established for any future
  viewport → window notification that would create a circular dependency.
- `pending_mesh_template_` being non-null is the single flag controlling
  placement mode; future creation tools can follow the same pattern with their
  own pending pointers.
