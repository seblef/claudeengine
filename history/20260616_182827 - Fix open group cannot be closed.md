# Fix: Open groups cannot be closed (#598)

## What changed

Single line change in `src/editor/EditorWindow.cpp` (toolbar state update block, ~line 242).

**Before:**
```cpp
toolbar_->SetCanCloseGroup(sel_group != nullptr && sel_group->is_open);
```

**After:**
```cpp
const ObjectGroup* close_grp = sel.empty() ? nullptr : scene_->FindGroup(sel[0]);
toolbar_->SetCanCloseGroup(close_grp != nullptr && close_grp->is_open);
```

## Root cause

`EditorScene::GetSelectionGroup()` explicitly returns `nullptr` for open groups:
```cpp
ObjectGroup* grp = FindGroup(selection_[0]);
if (!grp || grp->is_open) return nullptr;   // <-- rejects open groups
```

This is intentional: `GetSelectionGroup()` is designed for the closed-group use case (ungroup, select-all-members). The toolbar was using it to drive `can_close_group_`, which meant the flag was always `false` when a group was open.

## Decision

Rather than adding a new `GetOpenSelectionGroup()` method to `EditorScene`, the fix uses the existing `FindGroup(sel[0])` call — the same approach `CloseGroup()` itself already uses. This keeps the fix minimal and co-located with the similar `can_open_group_` logic above it.

## Files touched

- `src/editor/EditorWindow.cpp` — toolbar state update

## Output to keep in mind

- `GetSelectionGroup()` is for **closed** groups only. Anything related to open-group button state must use `FindGroup()` directly.
- The tooltip on the Close Group button already says "Select an open group first" — now it actually works.

## Skills / instructions used

- `src/CLAUDE.md` — include paths, Google C++ style guide
- `src/editor/CLAUDE.md` — GUI vs. edition logic separation (no logic in panel Render methods)
- Root `CLAUDE.md` — git workflow, conventional commits, history file format
