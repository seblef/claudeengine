# Editor Align Tool — feat #797

## Summary

Added an **Align Tool** to the editor toolbar that lets the user snap the
position of one or more selected objects to a chosen target object along any
combination of axes (X, Y, Z) using min/center/max reference points.

---

## User flow implemented

1. User selects one or more objects — the Align button is greyed out when nothing
   is selected (`toolbar_->SetCanAlign(!sel.empty())`).
2. User clicks the **Align** toolbar button (action button, not a toggle mode).
3. The viewport enters **pick-target mode**: hovering non-selected objects shows
   a **yellow** wireframe bbox; the cursor becomes a hand icon.  A hint overlay
   is shown at the top-left of the viewport.
4. User clicks the target object (selected objects are rejected as targets).
5. A **modal dialog** opens with the alignment options, pre-filled from the
   last use (options persist on the session-long `AlignTool` instance).
6. User adjusts options and clicks **Apply** — objects move and a
   `MultiTransformCommand` is pushed (fully undoable).  On **Cancel** or
   **Escape**, nothing moves.  Either way the previously active tool is restored.

---

## Architecture decisions

### AlignTool as EditorToolBase subclass (not a toolbar mode)

The Align button is rendered as an **action button** (like Fall to Terrain,
Center on Object) rather than a mode toggle.  This keeps the `EditorTool` enum
clean and avoids `prev_tool_` logic complications.  `EditorWindow::ActivateAlignTool()`
captures the current base tool pointer before activating `AlignTool`, and
`AlignTool::on_done_` restores it afterwards.

### Tool lifetime

`align_tool_` is created once in `EditorWindow`'s constructor and lives for the
session lifetime — this gives the alignment `AlignOptions` free session-level
persistence without any serialisation.

### Undo/redo via MultiTransformCommand

On Apply, all transformed objects are gathered into a single
`MultiTransformCommand::Entry` vector (before/after world transforms) and pushed
to the history.  No new command class was needed.

### Modal rendering from OnRender()

The modal is opened (`ImGui::OpenPopup`) and rendered (`ImGui::BeginPopupModal`)
directly inside `AlignTool::OnRender()`, which is called within the Viewport
ImGui window.  A boolean flag `open_popup_` triggers `OpenPopup` on the first
frame of `kShowingModal`.  The `Done()` path is always the last call in the
relevant code branch so `OnDeactivate()` fires cleanly after `OnRender()` returns.

### Yellow pick-hover bbox

Rather than re-using the white `DrawHoverBBox` from `PickingUtils`, the
`DrawPickHoverBBox` local helper draws the same 12-edge wireframe in
`IM_COL32(255, 220, 0, 200)` (yellow) to visually distinguish the pick-target
mode from normal selection hover.

---

## Files changed

| File | Change |
|------|--------|
| `src/editor/tools/AlignTool.h` | New — `EditorToolBase` subclass |
| `src/editor/tools/AlignTool.cpp` | New — pick-target phase, modal, alignment math, command push |
| `src/editor/tools/CMakeLists.txt` | Added `AlignTool.cpp` |
| `src/editor/EditorToolbar.h` | Added `SetOnAlign()`, `SetCanAlign()`, private members |
| `src/editor/EditorToolbar.cpp` | Added Align action button (ICON_FA_ALIGN_CENTER) |
| `src/editor/EditorWindow.h` | Added `align_tool_`, `align_prev_base_tool_`, `ActivateAlignTool()` |
| `src/editor/EditorWindow.cpp` | Wired callbacks, added `ActivateAlignTool()` implementation |

---

## Output to keep in mind

- `ImGuiMouseCursor_Crosshair` is **not available** in this ImGui build;
  `ImGuiMouseCursor_Hand` is used instead for the pick-target cursor.
- The Align button appears after the Group/Ungroup buttons and before the
  Reference Gauge button in the toolbar.
- `AlignOptions::group_mode` defaults to `false` (each-independently);
  the group mode row is hidden when only one object is selected.
- The pick-target phase ignores currently-selected objects as candidates —
  clicking a selected object is a no-op and does not open the modal.

---

## Skills files used

- `/impl-issue` — implementation workflow

## CLAUDE.md instructions followed

- One class per `.h` / `.cpp` pair
- `cppcheck-suppress unusedStructMember` on all private members used only in .cpp
- `std::find_if` preferred over raw index loops (no index loops used)
- Action button pattern consistent with existing `FallToTerrain` / `Group` buttons
- No comment blocks — only non-obvious WHY comments
- Google C++ style guide (cpplint clean)
