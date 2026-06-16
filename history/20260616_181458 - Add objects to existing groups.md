# Add objects to existing groups

**Issue**: #595
**Branch**: feat/editor-add-to-group-595
**Date**: 2026-06-16

## Summary

Previously the "Group" toolbar button was only enabled when all selected objects were ungrouped (to create a new group). It was impossible to add objects to an existing group without first ungrouping, re-selecting everything, and grouping again.

This contribution enables the "Group" button when the selection contains exactly one closed group (all its members) plus at least one additional ungrouped object. Clicking "Group" in that state adds the ungrouped objects to the existing group instead of creating a new one.

## Changes

### `src/editor/EditorScene.h` / `EditorScene.cpp`

Added two methods to `EditorScene`:

- **`AddToGroup(ObjectGroup*, vector<GameObject*>)`**: Appends objects to an existing group, skipping any that already belong to a group.
- **`FindAddToGroupTarget(vector<GameObject*>* out_ungrouped)`**: Inspects the current selection and returns the single closed group that is fully represented in it (all its members selected), along with the remaining ungrouped objects. Returns `nullptr` if the selection doesn't match this "add to group" pattern (e.g., objects from two groups, partial group selection, or open group).

### `src/editor/EditorWindow.cpp`

- **`Render()` — `can_group_` condition**: Extended to also enable the Group button when `FindAddToGroupTarget()` returns non-null.
- **`GroupObjects()`**: Tries `FindAddToGroupTarget()` first; if it succeeds, calls `AddToGroup()` and returns early. Falls through to the original "create new group" path otherwise.

## Design decisions

- **Closed group only**: Only closed groups can receive new members via this flow. An open group's members are individually selectable, so the "full group in selection" invariant cannot be reliably established.
- **No partial group in selection**: If only some members of a group are selected alongside other objects, `FindAddToGroupTarget()` returns nullptr, keeping the button disabled. This avoids ambiguity about which group the user intends to modify.
- **Objects from two groups**: If the selection spans two groups, `FindAddToGroupTarget()` returns nullptr. The user must ungroup explicitly.
- **Ungrouped objects already in a group are silently skipped in `AddToGroup()`**: Consistent with `CreateGroup()` behaviour.

## Testing notes

Scenario to verify:
1. Create objects A, B, C (ungrouped).
2. Select A + B → click Group → group "group1" is created.
3. Click elsewhere to deselect. Select group1 (click A or B with closed group).
4. Shift-click C → selection should be [A, B, C].
5. Group button should now be enabled.
6. Click Group → C is added to group1.
7. Ungroup group1 → A, B, C all become ungrouped.

## Skills and instructions used

- `impl-issue` skill
- `src/editor/CLAUDE.md`: GUI vs. edition logic separation (kept EditorScene responsible for group data queries, kept EditorWindow as the orchestrator)
- `src/CLAUDE.md`: one class per file, Google C++ style guide, include paths are project-relative
- Root `CLAUDE.md`: conventional commits, cpplint, history file, branch from dev
