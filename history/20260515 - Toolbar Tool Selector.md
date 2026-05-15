# Toolbar: Tool Selector Buttons

**Issue:** #174  
**Branch:** `feat/toolbar-tool-selector`  
**Date:** 2026-05-15

## Summary

Completes the `EditorToolbar` implementation: spec-compliant button labels, `PushID`/`PopID` for widget uniqueness, `SetActiveTool()` API, and keyboard shortcuts Q/W/E/R/C.

## Changes

### `src/editor/EditorToolbar.h`
- Added `SetActiveTool(EditorTool)` public method (was missing from the #173 stub).
- Removed the "stub: always kSelection until #174" note from `GetActiveTool()`.
- Updated class doc comment to describe the full contract.

### `src/editor/EditorToolbar.cpp`
- Button labels changed to spec: `[S]`, `[C]`, `[T]`, `[R]`, `[Z]`.
- Each button is now wrapped in `ImGui::PushID(i)` / `PopID()` for reliable widget identity.
- Active button highlighted with a distinct blue (`ImVec4(0.26, 0.59, 0.98, 1.0)`) via `PushStyleColor(ImGuiCol_Button, ...)`.
- Keyboard shortcuts handled at the start of `Render()` with `ImGui::IsKeyPressed(..., repeat=false)`:
  - `Q` → kSelection
  - `C` → kCamera
  - `W` → kTranslate
  - `E` → kRotate
  - `R` → kScale
- Shortcuts are suppressed when `ImGui::GetIO().WantTextInput` is true (user typing in a text field).

## Design Decisions

- **Shortcuts before Begin/End:** Keyboard input is checked before the `ImGui::Begin()` call so shortcuts work even when the toolbar window is collapsed or out of focus.
- **`WantTextInput` guard:** Prevents W/E/R/C from switching tools while the user types in an input widget elsewhere in the editor.
- **`repeat=false`:** Tool switching fires once per keypress, not repeatedly while the key is held.
- **`kActiveColour` constant:** Hard-coded to ImGui's default accent blue rather than deriving from `ImGuiCol_ButtonActive`, so the highlight is clearly visible regardless of theme.

## Skills / Instructions Used

- `impl-issue` skill
- `src/CLAUDE.md`: Google C++ style guide, one class per file
- `src/editor/CLAUDE.md`: editor is the leaf of the dependency graph
