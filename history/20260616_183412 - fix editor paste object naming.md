# fix(editor): Bad object name suffix increment when pasting object

**Issue**: #597
**Branch**: fix/editor-paste-object-naming-597
**Date**: 2026-06-16

## Problem

Two bugs in paste-object naming:

1. **Pasted object name**: `Copy()` appended `" (copy)"` to the clone's name. Each paste
   produced names like `"tree_001 (copy)"` or, on repeated paste, `"tree_001 (copy) (copy)"`.
   The expected behaviour is to assign the next available indexed name (e.g. `"tree_002"`),
   consistent with how newly placed objects are named.

2. **Pasted group name**: `PasteObject()` called `GenerateObjectName(*scene_, clipboard_group_name_)`
   where `clipboard_group_name_` already contained the full indexed name (e.g. `"group_001"`).
   `GenerateObjectName` then treated `"group_001"` as a base prefix and produced `"group_001_001"`
   instead of `"group_002"`.

## Changes

### `src/editor/ObjectNamingUtils.h` / `.cpp`

Added `BaseNameOf(const std::string& name)`:
- Strips a trailing `_NNN` (underscore followed only by digits) suffix.
- `"tree_001"` → `"tree"`, `"group_007"` → `"group"`, `"spotlight"` → `"spotlight"`.
- Used by `PasteObject()` to derive the base name for both object and group renaming.

### `src/game/GameMesh.cpp`, `GameLight.cpp`, `GameParticleSystem.cpp`

Removed the `+ " (copy)"` suffix from all `Copy()` implementations. The clone now
preserves the source name as a neutral starting point; the editor paste path is the
single place responsible for assigning a unique name.

### `src/editor/EditorWindow.cpp` — `PasteObject()`

1. After cloning each clipboard item, rename it:
   ```cpp
   clone->SetName(GenerateObjectName(*scene_, BaseNameOf(src->GetName())));
   ```
   Because `PlaceObjectCommand::Execute()` is called immediately inside `Push()`,
   each newly placed object is in the scene before the next clone is named, so
   consecutive pastes of objects sharing a base name each receive a distinct index.

2. Group name:
   ```cpp
   GenerateObjectName(*scene_, BaseNameOf(clipboard_group_name_), /*use_groups=*/true)
   ```
   Strips the `_NNN` suffix from the copied group name before generating the next index.

## Decisions

- Naming responsibility was moved entirely to the editor paste path rather than to
  `Copy()`. `Copy()` is an internal clone utility; naming is an editor concern.
- `BaseNameOf` is placed in `ObjectNamingUtils` (alongside `GenerateObjectName`)
  because it is part of the same naming convention.
- No changes to `CopySelectedObject()`: clipboard items retain their source names,
  which is the correct input for `BaseNameOf`.

## Things to keep in mind

- Pasting the same selection multiple times is safe: each iteration calls
  `GenerateObjectName` after the previous clone is in the scene, so indices advance
  monotonically.
- If an object's name does not follow the `_NNN` convention (e.g. plain `"spotlight"`),
  `BaseNameOf` returns it unchanged and `GenerateObjectName` produces `"spotlight_001"`.

## Skills / CLAUDE.md references used

- `impl-issue` skill
- `src/editor/CLAUDE.md`: GUI vs. edition logic separation, one class per file
- `src/CLAUDE.md`: Google C++ style, cpplint requirement, include root `src/`
