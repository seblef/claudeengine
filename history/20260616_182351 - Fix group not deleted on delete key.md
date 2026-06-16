# Fix: Group not deleted when its objects are deleted

## Issue

[#596](https://github.com/seblef/claudeengine/issues/596) — When the user selects a
closed group and presses Delete, all member objects are removed from the scene but the
`ObjectGroup` entry remains in `EditorScene::groups_`, leaving an orphaned empty group.

## Root cause

`SelectionTool::OnEvent` (Delete key handler) pushes one `DeleteObjectCommand` per
selected object. Each command calls `EditorScene::ReclaimDynamicObject`, which in turn
calls `RemoveFromGroup` — stripping the object from the group's `objects` vector one
by one. Once the loop ends, the `ObjectGroup` struct still exists in `groups_` with an
empty `objects` list.

## Fix

**File changed**: `src/editor/tools/SelectionTool.cpp`

Before the deletion loop, call `GetSelectionGroup()` to capture the group pointer while
the selection is still intact (the first `ReclaimDynamicObject` call would empty the
group's member list, making `GetSelectionGroup()` return `nullptr` mid-loop).

After the loop, if the captured group pointer is non-null and its `objects` list is now
empty (meaning all members were successfully deleted), call `scene_->DeleteGroup(sel_group)`.

The `objects.empty()` guard is the precise condition: if any member was non-dynamic (and
therefore skipped by `to_delete`), it would still be in `sel_group->objects`, and we
correctly leave the group intact.

## Decisions

- **No new command / no undo for group deletion**: group create/delete are already
  untracked by the command history (`GroupObjects` / `UngroupObjects` in `EditorWindow`
  call `scene_->CreateGroup` / `scene_->DeleteGroup` directly, without pushing a
  command). Keeping the same pattern is consistent and avoids over-engineering.
- **Capture group pointer before the loop**: `GetSelectionGroup()` checks selection size
  against group size — the first deletion modifies the group's object list, which would
  invalidate that check. Capturing up front is the right order.

## Output to keep in mind

- The group management APIs (`CreateGroup`, `DeleteGroup`, `GetSelectionGroup`) are not
  wrapped in undo/redo commands — this is an intentional design choice in the codebase.
  If undo/redo for grouping is needed in the future, a dedicated `GroupCommand` would be
  required.
- `ReclaimDynamicObject` already calls `RemoveFromGroup` for each object — the group's
  `objects` vector is authoritative for "which members are still alive".

## Skills / CLAUDE.md rules applied

- Followed the GUI vs. edition logic separation: the fix stays in `SelectionTool` (tool
  layer), not in any panel class.
- One class per `.cpp` / `.h` pair — no new files created, fix is a minimal delta.
- Commit message follows Conventional Commits (`fix(editor): ...`).
- Branch prefixed with `fix/` per the git workflow guidelines.
