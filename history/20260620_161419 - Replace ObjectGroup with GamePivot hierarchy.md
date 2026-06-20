# refactor(scene-graph): Replace ObjectGroup with GamePivot-based hierarchy

**Issue**: #657  
**PR**: #697  
**Branch**: `feat/scene-graph-objectgroup-pivot-657`  
**Date**: 2026-06-20

---

## What changed

Removed the `ObjectGroup` editor-only flat grouping concept and replaced it with first-class `GamePivot` nodes from the scene graph. Groups are now runtime objects that survive save/load and participate in the transform hierarchy.

### Deleted

- `src/editor/ObjectGroup.h` — struct and its singleton-style group list
- All `EditorScene` group methods: `CreateGroup`, `DeleteGroup`, `AddToGroup`, `RemoveFromGroup`, `FindGroup`, `GetSelectionGroup`, `FindAddToGroupTarget`
- Properties panel `RenderGroupProperties()` method
- Toolbar "Open Group" / "Close Group" buttons and their callbacks

### Added

- `EditorScene::GroupUnderNewPivot(name, objects)` — computes AABB centre, creates a pivot there, reparents selected objects under it
- `EditorScene::UngroupPivot(pivot)` — promotes children to pivot's parent, removes pivot
- `EditorScene::IsPivotExpanded(pivot)` / `SetPivotExpanded(pivot, bool)` — expand/collapse state stored in `expanded_pivots_: unordered_set<GamePivot*>`
- `src/editor/commands/GroupUnderPivotCommand.h/.cpp` — undoable group-under-pivot
- `src/editor/commands/UngroupPivotCommand.h/.cpp` — undoable ungroup
- `EditorWindow::GroupUnderPivot()` / `UngroupSelectedPivot()` / `CopySubtree()` — high-level editor actions
- `EditorWindow::clipboard_parent_names_` parallel to `clipboard_` for hierarchy-aware copy/paste

### Modified

| File | Change |
|---|---|
| `EditorScene.h/.cpp` | Removed all ObjectGroup APIs; added pivot grouping, expand state, `DetachChildren` free function |
| `ObjectsPanel.cpp` | Added `RenderPivotNode()` for tree display; root types skip parented objects |
| `MapSerializer.cpp` | Emits `parent:` field per object; removed `editor.groups` block |
| `MapLoader.cpp` | Second-pass parent-linking by name after all objects parsed |
| `EditorWindow.cpp` | New group/ungroup/copy-subtree logic; two-pass paste for hierarchy |
| `EditorToolbar.h/.cpp` | Removed Open/Close group buttons and their state |
| `TransformTool.cpp` | Multi-drag filters children whose parent is in the selection |
| `SelectionTool.cpp` | Removed post-delete `DeleteGroup` call |
| `ObjectNamingUtils.h/.cpp` | Removed unused `use_groups` parameter |

---

## Key decisions

### Expand/collapse state in EditorScene, not ObjectsPanel

Stored in `EditorScene::expanded_pivots_` (an `unordered_set`) so both the outliner (`ObjectsPanel`) and the selection policy (`SetSelectedObject`, `AddToSelection`) can read it from the same source. A collapsed pivot redirects any child selection to the pivot itself — identical to the old "closed group" UX.

### DetachChildren as a free function

`DetachChildren(obj)` promotes all children to `obj->GetParent()` before a pivot is removed. Originally declared as a private `EditorScene` method, cppcheck flagged it as static-eligible; moved to an anonymous namespace free function in `EditorScene.cpp` to avoid the warning without making it public.

### CopySubtree for hierarchy copy

`GamePivot::Copy()` stays flat (no recursive deep-copy of children) because the children's `unique_ptr` ownership doesn't fit the single-object `Copy(position)` API. Instead `CopySubtree()` in `EditorWindow` walks the subtree, copies each node individually, and records source→clone name pairs in `clipboard_parent_names_`. `PasteObject()` then re-links parents in a second pass.

### Serialization ordering

`MapLoader` does a second-pass link after all objects are instantiated so `parent:` references don't require specific ordering in the YAML file. Unknown parent names log a warning and the object becomes root-level.

---

## Output to keep in mind for future features

- `EditorScene::expanded_pivots_` is an in-memory set: it is NOT serialized. Pivots always open on load. If serializing expand state is ever needed, add `expanded_pivots:` to the `editor:` YAML section in `MapSerializer`.
- `GamePivot::Copy()` is still flat. If recursive copy is ever needed at the game layer (not editor layer), it should be added to `GamePivot::Copy()` rather than to the editor clipboard.
- The `parent:` YAML field is optional; legacy maps without it load correctly (all objects become root-level).
- `TransformTool` drag deduplication happens at drag-start only — the `drag_objects_` subset is computed once from the selection set at the moment the gizmo engages.

---

## Skills files used

- `projectSettings:impl-issue`

## CLAUDE.md rules applied

- One class per `.h`/`.cpp` pair (moved `DetachChildren` free function to `.cpp` anonymous namespace)
- Google C++ Style Guide (all cppcheck/cpplint clean)
- `cppcheck-suppress unusedStructMember` on all private members used only in `.cpp`
- Conventional commits message format
- History markdown in `history/` folder
- PR opened against `dev` with `Close #657`
