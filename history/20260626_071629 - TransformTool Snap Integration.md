# TransformTool Snap Integration

**Issue**: #810  
**PR**: #815  
**Branch**: `feat/transform-tool-snapping`  
**Date**: 2026-06-26

## What changed

Wired the `EditorToolbar` snap state into `TransformTool` so that all three
transform gizmos (translate, rotate, scale) snap to the configured granularities
when the snap toggle is enabled.

### Files modified

| File | Change |
|------|--------|
| `src/editor/tools/TransformTool.h` | Added `EditorToolbar` forward declaration, `SetToolbar()` setter, `BuildSnapArray()` private helper, `toolbar_` member |
| `src/editor/tools/TransformTool.cpp` | Added `#include "editor/EditorToolbar.h"`, implemented `BuildSnapArray()`, passed snap pointer to both `ImGuizmo::Manipulate()` calls |
| `src/editor/EditorWindow.cpp` | Called `SetToolbar(toolbar_.get())` on all three transform tools after construction |

## Design decisions

### Setter over EditorToolContext

`EditorToolbar` is not part of `EditorToolContext` and `EditorViewport` has no
reference to it. Adding toolbar to the context would require threading it through
`EditorViewport` (new `SetToolbar()` + member), which is disproportionate for a
read-only accessor. A `SetToolbar()` setter on `TransformTool` directly mirrors
`SetCommandHistory()` / `SetOnDirty()` patterns used elsewhere and keeps the
change scoped to the three files that need it.

### BuildSnapArray() helper

The snap-array logic is identical for single and multi-selection paths. Extracting
it into a private `BuildSnapArray(float snap[3])` method avoids duplication and
keeps both `ImGuizmo::Manipulate()` call sites clean (one extra line each).

The method returns `float*` (pointer into caller-provided `snap[3]`) or `nullptr`
when snap is disabled, matching ImGuizmo's own API convention.

### ImGuizmo snap semantics

- **Translate**: all three elements set to the same position-snap value (uniform
  grid). ImGuizmo applies the snap independently per axis.
- **Rotate**: only `snap[0]` is used by ImGuizmo regardless of array size.
- **Scale**: only `snap[0]` is used by ImGuizmo.

## Output to keep in mind

- `TransformTool::SetToolbar()` must be called from `EditorWindow` after both
  `toolbar_` and the tool instances are constructed. The current call site
  (just after `toolbar_->SetOnPlaceGauge(...)`) is correct because members are
  initialized in declaration order and all three are `unique_ptr` constructed in
  the initializer list before the constructor body runs.
- Undo/redo (`TransformCommand` / `MultiTransformCommand`) works correctly with
  snapped values because the commands snapshot the transform at drag-start and
  drag-end regardless of how ImGuizmo computed the intermediate positions.

## Skills and instructions applied

- `impl-issue` skill (branch from dev, cpplint, conventional commit, PR to dev)
- `src/editor/CLAUDE.md`: one class per file, forward declarations for pointer members
- `src/CLAUDE.md`: Google C++ style, include paths relative to `src/`
