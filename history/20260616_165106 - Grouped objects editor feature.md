# Grouped Objects — Editor Feature (#580)

## Summary

Implements grouped objects in the editor. Groups are a pure editor concept:
they affect selection and transform behaviour in the editor but have no
runtime representation in the game. Saved and loaded from the `editor` section
of the map YAML.

## Changes

### New files

| File | Role |
|---|---|
| `src/editor/ObjectGroup.h` | Plain struct holding group name, open/closed flag, and member pointers |

### Modified files

| File | Change |
|---|---|
| `src/editor/EditorScene.h/.cpp` | Group management API + group-aware selection expansion |
| `src/editor/ObjectNamingUtils.h/.cpp` | Optional `use_groups` flag to also scan group names for uniqueness |
| `src/editor/ObjectsPanel.h/.cpp` | Groups rendered as folder nodes; type-category nodes hide grouped objects |
| `src/editor/EditorToolbar.h/.cpp` | Four new group action buttons: Group, Ungroup, Open Group, Close Group |
| `src/editor/PropertiesPanel.h/.cpp` | `RenderGroupProperties()` shows only the group name when a closed group is selected |
| `src/editor/EditorWindow.h/.cpp` | Wires group actions, updates copy/paste to recreate groups, updates Properties rendering |
| `src/editor/MapSerializer.cpp` | Saves/loads `groups` sequence in the `editor` YAML block |

## Design decisions

### Groups as an editor-only concept
Groups live entirely in `EditorScene::groups_` — no changes to `game::GameObject`,
`GameSystem`, or any renderer. The serialiser writes them to the `editor:` section
of the map YAML so they survive save/load but are invisible to the runtime.

### Selection expansion (closed groups)
`EditorScene::SetSelectedObject` and `AddToSelection` check whether the target object
belongs to a closed group; if so, all group members are selected instead. This makes
clicking any member of a closed group behave as if the whole group was clicked,
covering both viewport picking (via `PickObjectAt`) and the Objects panel.

### Open vs. closed groups
- **Closed**: clicking any member selects the whole group; transform gizmo is
  positioned at the combined bounding-box centre; Properties shows only the group
  name.
- **Open**: members can be selected and modified independently, as if they were
  not grouped.

### Toolbar buttons
Four one-shot buttons added after the existing action buttons (Fall, Center):
`Group`, `Ungroup`, `Open Group`, `Close Group`. Each is greyed out when
inapplicable (conditions evaluated in `EditorWindow::Render` each frame).

### Copy/paste of groups
`CopySelectedObject` stores the group name in `clipboard_group_name_`. On paste,
if that name is non-empty and ≥2 objects were pasted, a new group is created
(with a unique generated name) and the pasted objects are automatically selected.

### Serialisation format
```yaml
editor:
  groups:
    - name: group_001
      open: false
      objects: [mesh1, mesh2, light1]
```
Groups with empty `objects` lists are silently skipped on load.

### Object deletion
`RemoveDynamicObject` and `ReclaimDynamicObject` now also call `RemoveFromGroup`
so deleted objects are automatically evicted from any group they belonged to.

## Key invariants to keep in mind

- `GetSelectionGroup()` returns non-null only when:
  1. The selection is non-empty
  2. The first selected object belongs to a closed group
  3. The selection size equals the group size
  4. All selected objects belong to the same group
  This is used to decide whether to show group properties and to enable toolbar actions.

- `GenerateObjectName` with `use_groups=true` must be used when generating group
  names to avoid collisions with existing group names (which are not game objects).

- `ObjectsPanel::RenderTypeGroup` filters out grouped objects so they appear only
  under their group's folder node, never duplicated in the type categories.

## Skills and instructions used

- `src/editor/CLAUDE.md`: GUI/edition logic separation; panel classes are pure UI;
  one class per `.h`/`.cpp` pair.
- `src/CLAUDE.md`: include root is `src/`, Google C++ style guide, meaningful docs.
- Root `CLAUDE.md`: conventional commits, history file format.
