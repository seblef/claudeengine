# Snap Modifier Key Toggle (Issue #813)

## Summary

Implemented the standard Ctrl-to-invert-snap UX pattern found in Blender, Unreal, and Unity. Holding Ctrl during a gizmo drag or object placement now temporarily inverts the global snap toggle for the duration of that interaction, without affecting the persistent setting.

## Changes

### `src/editor/EditorToolbar.h`
- Added `IsSnapEffectivelyEnabled() const` — returns `ctrl ? !snap_enabled_ : snap_enabled_`.
- `IsSnapEnabled()` retains its original meaning (persistent toggle state, used for config persistence and the button base appearance).

### `src/editor/EditorToolbar.cpp`
- Implemented `IsSnapEffectivelyEnabled()` reading `ImGui::GetIO().KeyCtrl`.
- Updated the snap toolbar button to show three visual states:
  - **Persistent ON, no Ctrl**: full teal (`kActiveColour`)
  - **Ctrl override ON** (snap was OFF, Ctrl pressed): dimmed teal (`kOverrideColour`, alpha 0.55) to signal the state is transient
  - **OFF**: default button colour
- Updated tooltips to mention the Ctrl behaviour.

### `src/editor/tools/TransformTool.cpp`
- `BuildSnapArray()`: changed `IsSnapEnabled()` → `IsSnapEffectivelyEnabled()`, so gizmo snapping follows the live Ctrl state.

### `src/editor/tools/PlacementTool.cpp`
- `UpdatePreviewPosition()`: changed `IsSnapEnabled()` → `IsSnapEffectivelyEnabled()`, so placement snapping follows the live Ctrl state.

### `src/editor/EditorViewport.cpp`
- `DrawSnapGrid()`: changed `IsSnapEnabled()` → `IsSnapEffectivelyEnabled()`, so the XZ grid overlay appears/disappears in response to the Ctrl override.

## Decisions

- **`IsSnapEffectivelyEnabled()` is the query point for all runtime snap decisions.** This keeps every consumer consistent without duplicating the XOR logic.
- **Dimmed tint for Ctrl override** rather than a separate icon or badge — it is subtle enough not to be distracting but clearly distinguishable from the solid teal of the persistent-on state.
- **No state change to `snap_enabled_`** — the Ctrl key is read live from ImGui's IO each frame; releasing Ctrl reverts behaviour on the very next frame with zero bookkeeping.
- **Tooltip update** — the tooltip now explains the Ctrl shortcut so new users can discover it without documentation.

## Acceptance criteria status

- [x] Holding Ctrl while dragging a gizmo inverts the snap toggle for that drag
- [x] Holding Ctrl during object placement inverts snap for that placement
- [x] Toolbar button reflects the modifier-key override state visually (dimmed teal)
- [x] Grid overlay follows the effective snap state
- [x] Releasing Ctrl restores the previous snap behavior immediately

## Skills / instructions used

- `impl-issue` skill
- `src/CLAUDE.md`: one class per file, Google C++ style, include root is `src/`
- `src/editor/CLAUDE.md`: GUI vs edition logic separation, `cppcheck-suppress unusedStructMember` pattern

## Notes for next features

- Any future snap consumer (e.g., grid-snapping road nodes, terrain brush alignment) should call `IsSnapEffectivelyEnabled()` rather than `IsSnapEnabled()`.
- If a second modifier key (e.g., Shift) is added for angle snapping override, the same XOR pattern applies — add a dedicated `IsAngleSnapEffectivelyEnabled()` in `EditorToolbar`.
