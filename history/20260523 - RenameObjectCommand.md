# RenameObjectCommand — 2026-05-23

## Overview

Added `RenameObjectCommand`, an undoable/redoable command for renaming `game::GameObject` instances in the editor.

## Changes

| File | Change |
|---|---|
| `src/editor/commands/RenameObjectCommand.h` | New header — declares the command class |
| `src/editor/commands/RenameObjectCommand.cpp` | New implementation — Execute sets new name, Undo restores old name |
| `src/editor/commands/CMakeLists.txt` | Registered `RenameObjectCommand.cpp` as a source |

## Decisions and rationale

- **`std::move` on constructor arguments**: `old_name` and `new_name` are taken by value and moved into members to avoid unnecessary copies.
- **No `Redo()` override**: `Redo()` defaults to `Execute()` via `EditorCommand`, which is correct here — re-applying the rename is identical to executing it.
- **`cppcheck-suppress unusedStructMember`**: Added per the pattern established by other commands in this module, suppressing cppcheck false positives on private members.

## Output to keep in mind

- `RenameObjectCommand` is now available for use in `ObjectsPanel` (inline rename on double-click) and `PropertiesPanel` (editable name field), both planned in upcoming issues.
- `game::GameObject::SetName(const std::string&)` and `GetName()` are the stable API surface used here.

## Skills and instructions used

- `impl-issue` skill workflow
- `src/CLAUDE.md`: one class per file, include root is `src/`, Google C++ style guide
- `src/editor/CLAUDE.md`: one class per `.h`/`.cpp` pair, editor is the dependency leaf
