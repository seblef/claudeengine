# EditorTool Enum and Toolbar: Object Creation Tool Buttons

**Issue:** #218
**Branch:** feat/editor-tool-creation-buttons
**Date:** 2026-05-16

## Summary

Extended `EditorTool` and `EditorToolbar` to support four object-creation modes
(mesh, omni light, circle-spot light, rectangle-spot light).

## Changes

### `src/editor/EditorTool.h`

- Added four new enum values after `kScale`:
  - `kCreateMesh`
  - `kCreateOmniLight`
  - `kCreateCircleSpot`
  - `kCreateRectSpot`
- Added `IsCreationTool(EditorTool)` free function to centralise the "is this a
  creation tool?" check — avoids repetitive switch/if chains in callers.

### `src/editor/EditorToolbar.cpp`

- Split the flat `kTools[]` array into `kTransformTools[]` (with shortcuts) and
  `kCreationTools[]` (toolbar-only, no shortcuts).
- Extracted `RenderToolButton()` helper to avoid duplicating the
  PushID/PushStyleColor/Button/SetItemTooltip/PopStyleColor pattern.
- Added `ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical)` between the two groups.
  `SeparatorEx` lives in `imgui_internal.h` — included accordingly.
- Keyboard shortcut loop now only iterates over transform tools.

### `src/editor/EditorToolbar.h`

- Updated class doc-comment to reflect the two groups and the separator.

### `src/editor/EditorWindow.h`

- Added `#include "editor/EditorTool.h"` (needed for `prev_tool_` field).
- Added `EditorTool prev_tool_` to track tool transitions across frames.

### `src/editor/EditorWindow.cpp`

- `SetSelectionActive` is now `true` only for `kSelection` (was restricted to
  `IsSelectionToolActive()` which was already equivalent — made explicit).
- Detects transitions into any creation tool and logs a placeholder message
  (placement/modal implementation is deferred to follow-up issues).
- Captured `active_tool` into a local variable to avoid two `GetActiveTool()` calls.

## Decisions and Rationale

### `IsCreationTool()` as a free function in the header

Putting the helper inline in `EditorTool.h` means every consumer can use it without
extra includes or a utility header. It's a pure predicate with no state, so it
belongs next to the enum definition.

### `imgui_internal.h` for `SeparatorEx`

`ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical)` is the canonical way to draw a
vertical separator in horizontal-layout ImGui code, but it is internal API.
The alternative — `ImGui::Text("|")` or a manual draw-list line — is fragile
(text carries font metrics; draw-list coords are error-prone). Using
`imgui_internal.h` is acceptable here because the editor already relies heavily
on ImGui internals (e.g., ImGuizmo uses them too) and the ImGui version is pinned.

### `ToolToImGuizmoOp` unchanged

The existing `default: return std::nullopt` branch already handles the four new
tools correctly — no gizmo while in creation mode.

### Transition logging (NYI placement/modal)

The MeshSelectionModal and placement-mode logic are out of scope for this PR.
A `LOG_F(INFO, ...)` placeholder is emitted on transition so the behaviour is
observable during development. Callers are expected to replace this with real
modal/placement code in follow-up issues.

## Output to Keep in Mind

- `IsCreationTool()` is the preferred way to test for creation tools; extend it
  when adding more creation types.
- `EditorWindow::prev_tool_` is available for future transition handlers — no
  additional field needed.
- `imgui_internal.h` is now included by `EditorToolbar.cpp`; keep this in mind
  if the ImGui version is ever upgraded.

## Skills and Instructions Used

- `impl-issue` skill (CLAUDE.md Git workflow, conventional commits, history file)
- `src/editor/CLAUDE.md` (one class per file, Google C++ style guide, ImGui lifecycle)
